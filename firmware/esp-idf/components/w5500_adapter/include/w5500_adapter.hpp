#pragma once
class W5500Adapter {
public:
    bool begin();
    [[nodiscard]] bool link_up() const;
    [[nodiscard]] bool initialized() const { return initialized_; }
private:
    bool initialized_{};
    bool link_up_{};
};
