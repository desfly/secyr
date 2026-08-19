#!/usr/bin/env python3
"""Static regression contract for HomeGuard-S3 sidebar navigation state ownership."""
from __future__ import annotations

import re
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
EXPECTED_ROUTES = (
    "#overview",
    "#zones-section",
    "#zones",
    "#io-section",
    "#ioState",
    "#events",
    "#history",
    "#networkPage",
    "#system",
)


class Parser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.in_nav = False
        self.hrefs: list[str] = []
        self.ids: set[str] = set()
        self.group_labels: list[tuple[str, set[str]]] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        element_id = values.get("id")
        if element_id:
            self.ids.add(element_id)
        if tag == "nav":
            self.in_nav = True
            return
        if not self.in_nav:
            return
        classes = set((values.get("class") or "").split())
        if tag == "a":
            self.hrefs.append(values.get("href") or "")
        if "nav-group-label" in classes:
            self.group_labels.append((tag, classes))

    def handle_endtag(self, tag: str) -> None:
        if tag == "nav":
            self.in_nav = False


def css_rules(css: str) -> list[tuple[str, str]]:
    return [(m.group(1).strip(), m.group(2)) for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css)]


def main() -> int:
    index = (WEB / "index.html").read_text(encoding="utf-8")
    css = (WEB / "app.css").read_text(encoding="utf-8")
    app_js = (WEB / "app.js").read_text(encoding="utf-8")
    access_js = (WEB / "access-session.js").read_text(encoding="utf-8")
    reset_js = (WEB / "factory-reset.js").read_text(encoding="utf-8")

    parser = Parser()
    parser.feed(index)
    errors: list[str] = []

    if tuple(parser.hrefs) != EXPECTED_ROUTES:
        errors.append(f"sidebar routes changed/reordered: {parser.hrefs}")
    if len(parser.hrefs) != len(set(parser.hrefs)):
        errors.append(f"duplicate sidebar hrefs: {parser.hrefs}")
    for route in EXPECTED_ROUTES:
        target = route[1:]
        if target != "overview" and target not in parser.ids:
            errors.append(f"route target missing: {route}")

    if len(parser.group_labels) != 1:
        errors.append(f"expected exactly one settings group label, got {parser.group_labels}")
    else:
        tag, classes = parser.group_labels[0]
        if tag == "a":
            errors.append("settings group label must not be an anchor")
        if "active" in classes:
            errors.append("settings group label must never be statically active")

    rules = css_rules(css)
    active_rules = [(selector, body) for selector, body in rules if "nav a.active" in selector]
    if len(active_rules) != 1:
        errors.append(f"expected exactly one nav a.active CSS rule, got {len(active_rules)}")
    elif "background" not in active_rules[0][1]:
        errors.append("nav a.active rule no longer owns a background highlight")

    forbidden_state = re.compile(r"nav\s+a(?::(?:hover|focus|focus-visible|active)|\.selected)")
    for selector, body in rules:
        if forbidden_state.search(selector) and re.search(r"(?:^|;)\s*background(?:-image)?\s*:", body):
            errors.append(f"non-authoritative nav state paints a background highlight: {selector}")

    scripts = {"app.js": app_js, "access-session.js": access_js, "factory-reset.js": reset_js}
    active_mutators: list[str] = []
    for name, source in scripts.items():
        for needle in ("classList.add(\"active\")", "classList.add('active')", "className = \"active\"", "className='active'"):
            if needle in source:
                active_mutators.append(f"{name}:{needle}")
    if active_mutators:
        errors.append(f"competing active-state writers found: {active_mutators}")

    toggle_count = app_js.count('classList.toggle("active"') + app_js.count("classList.toggle('active'")
    if toggle_count != 1:
        errors.append(f"expected one authoritative active toggle in app.js, got {toggle_count}")

    if "window.addEventListener(\"hashchange\", routeFromHash)" not in app_js:
        errors.append("hashchange routing is not wired to routeFromHash")
    if "window.location.hash === href" not in app_js:
        errors.append("same-hash click repair path is missing")

    if errors:
        print("Web navigation state contract FAIL")
        for error in errors:
            print(f" - {error}")
        return 1

    print("Web navigation state contract PASS")
    print(f" - unique sidebar routes: {len(EXPECTED_ROUTES)}")
    print(" - settings is a non-clickable group label")
    print(" - blue background ownership is reserved for nav a.active")
    print(" - no competing JS active-state writer detected")
    print(" - hashchange + same-hash routing contract present")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
