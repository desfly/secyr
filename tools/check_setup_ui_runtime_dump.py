#!/usr/bin/env python3
"""Validate the DOM marker produced by the setup UI browser smoke harness."""
from __future__ import annotations

import argparse
from html.parser import HTMLParser
from pathlib import Path


class HtmlAttributes(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.html: dict[str, str] | None = None

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag == "html" and self.html is None:
            self.html = {name: value or "" for name, value in attrs}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dom", type=Path)
    parser.add_argument("--viewport", choices=("desktop", "mobile"), required=True)
    args = parser.parse_args()

    document = HtmlAttributes()
    document.feed(args.dom.read_text(encoding="utf-8"))
    attrs = document.html or {}
    expected = {
        "data-setup-ui-smoke": "done",
        "data-setup-ui-viewport": args.viewport,
        "data-setup-ui-columns": "1" if args.viewport == "mobile" else "2",
        "data-setup-ui-wifi-rows": "2",
        "data-setup-ui-setup-eyes": "2",
        "data-setup-ui-login-eyes": "1",
    }
    errors = [f"{name}: expected {value!r}, got {attrs.get(name)!r}"
              for name, value in expected.items() if attrs.get(name) != value]
    if attrs.get("data-setup-ui-error"):
        errors.append(f"browser harness: {attrs['data-setup-ui-error']}")
    if errors:
        print(f"Setup UI runtime {args.viewport} FAIL")
        for error in errors:
            print(f" - {error}")
        return 1
    print(f"Setup UI runtime {args.viewport} PASS")
    print(f" - responsive columns: {expected['data-setup-ui-columns']}")
    print(" - Wi-Fi scan rows and selection behavior: verified")
    print(" - setup Wi-Fi/Admin and login password eyes: verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
