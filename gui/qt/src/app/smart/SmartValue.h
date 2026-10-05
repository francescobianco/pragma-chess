#pragma once

#include <QString>

#include <memory>
#include <vector>

/// Something a client hands a SMART program — a position, an evaluation, a
/// line —, used only through the client's functions.
class SmartObject {
public:
    virtual ~SmartObject() = default;
    /// What it is, for error messages: "position", "evaluation"…
    virtual QString typeName() const = 0;
};

/// A value of a SMART program (smart/README.md): a number, a text, a list,
/// NOTHING, or an object of the client. Lists are values: copying one and
/// changing the copy leaves the original alone.
class SmartValue {
public:
    enum class Type { Nothing, Number, Text, List, Object };

    SmartValue() = default;
    SmartValue(double number);
    SmartValue(int number);
    SmartValue(bool truth);
    SmartValue(const QString &text);
    SmartValue(std::vector<SmartValue> items);
    SmartValue(std::shared_ptr<SmartObject> object);

    Type type() const { return m_type; }
    bool isNothing() const { return m_type == Type::Nothing; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isText() const { return m_type == Type::Text; }
    bool isList() const { return m_type == Type::List; }
    bool isObject() const { return m_type == Type::Object; }

    double number() const { return m_number; }
    const QString &text() const { return m_text; }
    const std::vector<SmartValue> &items() const;
    /// The list's elements to change: the list is copied first if another
    /// value shares them.
    std::vector<SmartValue> &mutableItems();
    const std::shared_ptr<SmartObject> &object() const { return m_object; }
    /// The object as the client's class, or null.
    template <typename T>
    std::shared_ptr<T> as() const
    {
        return std::dynamic_pointer_cast<T>(m_object);
    }

    /// "number", "text", "list", "nothing", or the object's own name.
    QString typeName() const;
    /// The value as STR and SAY write it: numbers whole when they are,
    /// otherwise with at most 6 decimals and no trailing zeros.
    QString toText() const;
    static QString numberText(double number);

    /// Same type and same value; objects are equal only to themselves.
    bool operator==(const SmartValue &other) const;

private:
    Type m_type = Type::Nothing;
    double m_number = 0;
    QString m_text;
    std::shared_ptr<std::vector<SmartValue>> m_items;
    std::shared_ptr<SmartObject> m_object;
};
