#include "SmartValue.h"

#include <QStringList>

#include <cmath>

SmartValue::SmartValue(double number)
    : m_type(Type::Number)
    , m_number(number)
{
}

SmartValue::SmartValue(int number)
    : SmartValue(double(number))
{
}

SmartValue::SmartValue(bool truth)
    : SmartValue(truth ? 1.0 : 0.0)
{
}

SmartValue::SmartValue(const QString &text)
    : m_type(Type::Text)
    , m_text(text)
{
}

SmartValue::SmartValue(std::vector<SmartValue> items)
    : m_type(Type::List)
    , m_items(std::make_shared<std::vector<SmartValue>>(std::move(items)))
{
}

SmartValue::SmartValue(std::shared_ptr<SmartObject> object)
    : m_type(object ? Type::Object : Type::Nothing)
    , m_object(std::move(object))
{
}

const std::vector<SmartValue> &SmartValue::items() const
{
    static const std::vector<SmartValue> none;
    return m_items ? *m_items : none;
}

std::vector<SmartValue> &SmartValue::mutableItems()
{
    if (!m_items)
        m_items = std::make_shared<std::vector<SmartValue>>();
    else if (m_items.use_count() > 1)
        m_items = std::make_shared<std::vector<SmartValue>>(*m_items);
    return *m_items;
}

QString SmartValue::typeName() const
{
    switch (m_type) {
    case Type::Nothing: return QStringLiteral("nothing");
    case Type::Number: return QStringLiteral("number");
    case Type::Text: return QStringLiteral("text");
    case Type::List: return QStringLiteral("list");
    case Type::Object: return m_object->typeName();
    }
    return {};
}

QString SmartValue::numberText(double number)
{
    if (std::isfinite(number) && number == std::trunc(number) && std::abs(number) < 1e15)
        return QString::number(qint64(number));
    QString text = QString::number(number, 'f', 6);
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

QString SmartValue::toText() const
{
    switch (m_type) {
    case Type::Nothing: return QStringLiteral("NOTHING");
    case Type::Number: return numberText(m_number);
    case Type::Text: return m_text;
    case Type::List: {
        QStringList parts;
        for (const SmartValue &item : items())
            parts << (item.isText() ? QLatin1Char('"') + item.text() + QLatin1Char('"') : item.toText());
        return QLatin1Char('[') + parts.join(QStringLiteral(", ")) + QLatin1Char(']');
    }
    case Type::Object: return QLatin1Char('<') + m_object->typeName() + QLatin1Char('>');
    }
    return {};
}

bool SmartValue::operator==(const SmartValue &other) const
{
    if (m_type != other.m_type)
        return false;
    switch (m_type) {
    case Type::Nothing: return true;
    case Type::Number: return m_number == other.m_number;
    case Type::Text: return m_text == other.m_text;
    case Type::List: return items() == other.items();
    case Type::Object: return m_object == other.m_object;
    }
    return false;
}
