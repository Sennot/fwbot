#pragma once

#include <cctype>
#include <charconv>
#include <cmath>
#include <glaze/glaze.hpp>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

enum class KeybindType { Hold = 0, Toggle, Override };

struct RawKeybind {
    int m_key;
    int m_modifiers;
    std::string m_valueTag;

    KeybindType m_type;
    bool m_active = true;
    std::string m_value;
};

// clang-format off
template <>
struct glz::meta<RawKeybind> {
    using T = RawKeybind;
    static constexpr auto value = object(
        "key", &T::m_key,
        "modifiers", &T::m_modifiers,
        "tag", &T::m_valueTag,
        "type", &T::m_type,
        "value", &T::m_value,
        "active", &T::m_active
    );
};
// clang-format on

namespace keybind {

// A keybind writes through a void* as T&, so it has to agree with the value it
// was looked up for. Comparing these gives that check without needing RTTI.
using TypeKey = const void*;

template <typename T>
inline TypeKey typeKey() {
    static const char id{};
    return &id;
}

inline std::string_view trimSpace(std::string_view str) {
    while (!str.empty() &&
           std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }

    while (!str.empty() &&
           std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }

    return str;
}

/**
 * Strict numeric parse - the whole string has to be a number, so trailing junk
 * reads as the typo it is instead of silently becoming a value.
 *
 * Works for both integral and floating point Ts.
 */
template <typename T>
inline bool parseNumber(std::string_view str, T& out) {
    str = trimSpace(str);
    if (!str.empty() && str.front() == '+') str.remove_prefix(1);
    if (str.empty()) return false;

    const char* end = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(str.data(), end, out);

    return ec == std::errc{} && ptr == end;
}

enum Modifier : int {
    Shift = 0b100,
    Ctrl = 0b010,
    Alt = 0b001,
};

inline int packModifiers(bool ctrl, bool shift, bool alt) {
    return (ctrl ? Ctrl : 0) | (shift ? Shift : 0) | (alt ? Alt : 0);
}

inline int makeHash(int key, int modifiers) { return key | (modifiers << 20); }

inline bool isModifierKey(int key) {
    switch (key) {
        case 0x10:  // VK_SHIFT
        case 0x11:  // VK_CONTROL
        case 0x12:  // VK_MENU
        case 0xA0:  // VK_LSHIFT
        case 0xA1:  // VK_RSHIFT
        case 0xA2:  // VK_LCONTROL
        case 0xA3:  // VK_RCONTROL
        case 0xA4:  // VK_LMENU
        case 0xA5:  // VK_RMENU
            return true;
        default:
            return false;
    }
}

inline std::string keyName(int key) {
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z'))
        return std::string(1, static_cast<char>(key));
    if (key >= 0x70 && key <= 0x87)  // VK_F1 .. VK_F24
        return "F" + std::to_string(key - 0x6F);

    switch (key) {
        case 0x08:
            return "Backspace";
        case 0x09:
            return "Tab";
        case 0x0D:
            return "Enter";
        case 0x1B:
            return "Escape";
        case 0x20:
            return "Space";
        case 0x21:
            return "Page Up";
        case 0x22:
            return "Page Down";
        case 0x23:
            return "End";
        case 0x24:
            return "Home";
        case 0x25:
            return "Left";
        case 0x26:
            return "Up";
        case 0x27:
            return "Right";
        case 0x28:
            return "Down";
        case 0x2D:
            return "Insert";
        case 0x2E:
            return "Delete";
        case 0x97:
            return "Wheel Up";
        case 0x98:
            return "Wheel Down";
        default:
            return "Key " + std::to_string(key);
    }
}

inline std::string keyLabel(int key, int modifiers) {
    std::string label;
    if (modifiers & Ctrl) label += "Ctrl + ";
    if (modifiers & Shift) label += "Shift + ";
    if (modifiers & Alt) label += "Alt + ";
    label += keyName(key);
    return label;
}

}  // namespace keybind

class KeybindControl {
   protected:
    KeybindType m_type;
    bool m_enabled = false;
    bool m_active = true;

   public:
    std::string m_valueTag;
    int m_key;
    int m_modifiers;

    using HashT = int;

    HashT getHash() { return keybind::makeHash(m_key, m_modifiers); }
    const std::string& getTag() { return m_valueTag; }
    KeybindType getType() const { return m_type; }
    bool isActive() const { return m_active; }
    std::string getKeyLabel() { return keybind::keyLabel(m_key, m_modifiers); }

    // identifies the T this control casts `value`/`previous` to
    virtual keybind::TypeKey getTypeKey() const = 0;

    virtual void applyValue(bool pressed, void* value, void* previous) = 0;
    virtual void fromString(const std::string& str) = 0;
    const virtual std::string toString() = 0;

    virtual bool canParse(const std::string& str) = 0;

    virtual ~KeybindControl() = default;

    friend glz::meta<KeybindControl>;
};

template <typename T>
class SLKeybind : public KeybindControl {
    using Self = SLKeybind<T>;

   public:
    /**
     * Behavior explanation:
     *
     * ## Hold
     * - The keybind will only be active while the key is being held down
     * - The keybind will be disabled when the key is released
     * - If the value is already set to the keybind's value, it won't be changed
     *
     * ## Toggle
     * - The keybind will be activated when the key is pressed
     * - The keybind will be deactivated when the key is pressed again
     * - If the value is already set to the keybind's value, it'll go back to
     * the previous known value
     *
     * ## Override
     * - Set the value on key press, but don't change it back
     */

   private:
    // What value to set when the keybind is pressed
    T m_value;

   public:
    SLKeybind(int key, int modifiers, KeybindType type, T value,
              std::string tag)
        : m_value(value) {
        m_key = key;
        m_modifiers = modifiers;
        m_valueTag = tag;
        m_type = type;
    }

    static std::shared_ptr<SLKeybind<T>> create(std::string tag, int key,
                                                KeybindType type, T value,
                                                int modifiers = 0) {
        return std::make_shared<SLKeybind<T>>(key, modifiers, type, value, tag);
    }

    static std::shared_ptr<SLKeybind<T>> createFromString(std::string tag,
                                                          int key,
                                                          KeybindType type,
                                                          std::string value,
                                                          int modifiers = 0) {
        return std::make_shared<SLKeybind<T>>(key, modifiers, type,
                                              Self::readFromString(value), tag);
    }

    keybind::TypeKey getTypeKey() const override {
        return keybind::typeKey<T>();
    }

    void applyValue(bool pressed, void* value, void* previous) override {
        if (!m_active) return;

        T& valueRef = *(T*)value;
        T& previousRef = *(T*)previous;

        switch (m_type) {
            case KeybindType::Toggle: {
                if (!pressed) return;

                if (valueRef == m_value) {
                    valueRef = previousRef;
                } else {
                    previousRef = valueRef;
                    valueRef = m_value;
                }

                break;
            }
            case KeybindType::Hold: {
                if (!m_enabled && pressed) {
                    previousRef = valueRef;
                    valueRef = m_value;
                    m_enabled = true;
                } else if (m_enabled && !pressed) {
                    valueRef = previousRef;
                    previousRef = m_value;
                    m_enabled = false;
                }

                break;
            }
            case KeybindType::Override: {
                previousRef = valueRef;
                valueRef = m_value;
                break;
            }
        }
    }

    /**
     * Every value is typed into the same text box, and the type behind a tag
     * isn't obvious from the UI, so any number has to parse into any numeric T:
     * "2" for a double, "2.0" or "1e3" for an int. Anything that doesn't fit T
     * is rejected rather than truncated to something the user didn't ask for.
     */
    static bool parse(const std::string& str, T& out) {
        if constexpr (std::is_same_v<T, std::string>) {
            out = str;
            return true;
        } else if constexpr (std::is_same_v<T, bool>) {
            // bools are written as 1/0, but accept any number and true/false
            double number = 0.0;
            if (keybind::parseNumber(str, number)) {
                out = number != 0.0;
                return true;
            }

            std::istringstream alpha(str);
            return static_cast<bool>(alpha >> std::boolalpha >> out);
        } else if constexpr (std::is_arithmetic_v<T>) {
            // exact for integers too large to survive a double
            if constexpr (std::is_integral_v<T>) {
                if (keybind::parseNumber(str, out)) return true;
            }

            double number = 0.0;
            if (!keybind::parseNumber(str, number)) return false;

            if constexpr (std::is_integral_v<T>) {
                if (!std::isfinite(number)) return false;
                number = std::trunc(number);
            }

            if (std::isfinite(number) &&
                (number < static_cast<double>(std::numeric_limits<T>::lowest()) ||
                 number > static_cast<double>(std::numeric_limits<T>::max()))) {
                return false;
            }

            out = static_cast<T>(number);
            return true;
        } else {
            std::istringstream iss(str);
            return static_cast<bool>(iss >> out);
        }
    }

    static T readFromString(const std::string& str) {
        T value{};
        (void)Self::parse(str, value);
        return value;
    }

    // keeps the current value if the string can't be parsed
    void fromString(const std::string& str) override {
        T value{};
        if (Self::parse(str, value)) m_value = value;
    }

    bool canParse(const std::string& str) override {
        T value{};
        return Self::parse(str, value);
    }

    const std::string toString() override {
        // the stream would round to 6 significant digits, so editing an
        // unrelated field of a float keybind would quietly change its value
        if constexpr (std::is_floating_point_v<T>) {
            char buf[64];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), m_value);
            if (ec == std::errc{}) return std::string(buf, ptr);
        }

        std::ostringstream oss;
        oss << m_value;
        return oss.str();
    }

    friend glz::meta<SLKeybind<T>>;
};

// clang-format off
template <>
struct glz::meta<KeybindControl> {
    using T = KeybindControl;
    static constexpr auto value = object(
        "key", &T::m_key,
        "modifiers", &T::m_modifiers,
        "tag", &T::m_valueTag,
        "type", &T::m_type,
        "value", custom<&T::fromString, &T::toString>,
        "active", &T::m_active,
        "enabled", hide{&T::m_enabled}
    );
};
// clang-format on

template <typename T>
using KeybindPtr = std::shared_ptr<SLKeybind<T>>;

template <typename T>
class KeybindContainer {
   protected:
    std::string m_tag;
    std::vector<SLKeybind<T>> m_keybinds;

   public:
    std::vector<SLKeybind<T>>& getKeybinds() { return m_keybinds; }
};
