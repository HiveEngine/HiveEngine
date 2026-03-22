#pragma once

#include <queen/reflect/field_attributes.h>
#include <queen/reflect/field_info.h>

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

#include <cmath>

namespace hive::math
{
    struct Float3;
    struct Quat;
}

namespace forge
{
    constexpr float DEG_PER_RAD = 180.f / 3.14159265358979323846f;
    constexpr float RAD_PER_DEG = 3.14159265358979323846f / 180.f;

    constexpr const char* AXIS_LABELS[] = {"X", "Y", "Z", "W"};
    constexpr const char* AXIS_COLORS[] = {"#e05555", "#55b855", "#5580e0", "#c0c0c0"};
    constexpr const char* DEG_SUFFIX = "\xC2\xB0";

    struct AxisSpinConfig
    {
        double m_value;
        double m_step{0.01};
        double m_min{-1e9};
        double m_max{1e9};
        const char* m_suffix{nullptr};
    };

    [[nodiscard]] inline const char* GetDisplayName(const queen::FieldInfo& field) noexcept
    {
        if (field.m_attributes != nullptr && field.m_attributes->m_displayName != nullptr)
            return field.m_attributes->m_displayName;
        return field.m_name;
    }

    [[nodiscard]] inline bool HasFlag(const queen::FieldInfo& field, queen::FieldFlag flag) noexcept
    {
        return field.m_attributes != nullptr && field.m_attributes->HasFlag(flag);
    }

    inline void ApplyTooltip(QWidget* widget, const queen::FieldInfo& field)
    {
        if (field.m_attributes != nullptr && field.m_attributes->m_tooltip != nullptr)
            widget->setToolTip(QString::fromUtf8(field.m_attributes->m_tooltip));
    }

    inline void ApplyRange(QDoubleSpinBox* spin, const queen::FieldInfo& field)
    {
        if (field.m_attributes != nullptr && field.m_attributes->HasRange())
        {
            spin->setMinimum(static_cast<double>(field.m_attributes->m_min));
            spin->setMaximum(static_cast<double>(field.m_attributes->m_max));
        }
        else
        {
            spin->setMinimum(-1e9);
            spin->setMaximum(1e9);
        }
    }

    [[nodiscard]] inline QDoubleSpinBox* MakeDoubleSpinBox(double value, double step = 0.01)
    {
        auto* spin = new QDoubleSpinBox;
        spin->setDecimals(4);
        spin->setSingleStep(step);
        spin->setMinimum(-1e9);
        spin->setMaximum(1e9);
        spin->setValue(value);
        return spin;
    }

    template <typename Callback>
    QWidget* BuildAxisRow(int axisCount, const AxisSpinConfig* configs, Callback&& onChanged)
    {
        auto* container = new QWidget;
        auto* hbox = new QHBoxLayout{container};
        hbox->setContentsMargins(0, 0, 0, 0);
        hbox->setSpacing(2);

        for (int axis = 0; axis < axisCount; ++axis)
        {
            auto* axisLabel = new QLabel{AXIS_LABELS[axis]};
            axisLabel->setFixedWidth(12);
            axisLabel->setAlignment(Qt::AlignCenter);
            axisLabel->setStyleSheet(
                QString{"color: %1; font-size: 10px; font-weight: bold;"}.arg(AXIS_COLORS[axis]));
            hbox->addWidget(axisLabel);

            auto* spin = MakeDoubleSpinBox(configs[axis].m_value, configs[axis].m_step);
            spin->setMinimum(configs[axis].m_min);
            spin->setMaximum(configs[axis].m_max);
            if (configs[axis].m_suffix != nullptr)
                spin->setSuffix(QString::fromUtf8(configs[axis].m_suffix));
            hbox->addWidget(spin, 1);

            QObject::connect(spin, &QDoubleSpinBox::editingFinished, container,
                             [spin, axis, cb = std::forward<Callback>(onChanged)]() {
                                 cb(axis, static_cast<float>(spin->value()));
                             });
        }

        return container;
    }

    inline void QuatToEuler(const float* quat, float* euler)
    {
        const float sinr = 2.f * (quat[3] * quat[0] + quat[1] * quat[2]);
        const float cosr = 1.f - 2.f * (quat[0] * quat[0] + quat[1] * quat[1]);
        euler[0] = std::atan2(sinr, cosr) * DEG_PER_RAD;

        const float sinp = 2.f * (quat[3] * quat[1] - quat[2] * quat[0]);
        euler[1] = std::fabs(sinp) >= 1.f ? std::copysign(90.f, sinp) : std::asin(sinp) * DEG_PER_RAD;

        const float siny = 2.f * (quat[3] * quat[2] + quat[0] * quat[1]);
        const float cosy = 1.f - 2.f * (quat[1] * quat[1] + quat[2] * quat[2]);
        euler[2] = std::atan2(siny, cosy) * DEG_PER_RAD;
    }

    inline void EulerToQuat(const float* euler, float* quat)
    {
        const float p = euler[0] * RAD_PER_DEG * 0.5f;
        const float y = euler[1] * RAD_PER_DEG * 0.5f;
        const float r = euler[2] * RAD_PER_DEG * 0.5f;
        const float cp = std::cos(p), sp = std::sin(p);
        const float cy = std::cos(y), sy = std::sin(y);
        const float cr = std::cos(r), sr = std::sin(r);
        quat[0] = sr * cp * cy - cr * sp * sy;
        quat[1] = cr * sp * cy + sr * cp * sy;
        quat[2] = cr * cp * sy - sr * sp * cy;
        quat[3] = cr * cp * cy + sr * sp * sy;
    }
} // namespace forge
