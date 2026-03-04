#include "Lvs/Studio/Widgets/Properties/PathEditor.hpp"

#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMetaType>
#include <QPushButton>

namespace Lvs::Studio::Widgets::Properties {

PathEditor::PathEditor(
    const QString& value,
    const QVariantMap& customAttributes,
    std::function<void(const QString&)> onChanged,
    QWidget* parent
)
    : QWidget(parent),
      customAttributes_(customAttributes),
      onChanged_(std::move(onChanged)) {
    lineEdit_ = new QLineEdit(this);
    browseButton_ = new QPushButton("...", this);
    browseButton_->setFixedWidth(26);
    browseButton_->setFocusPolicy(Qt::NoFocus);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(lineEdit_, 1);
    layout->addWidget(browseButton_);

    setFocusProxy(lineEdit_);
    setPath(value);

    QObject::connect(lineEdit_, &QLineEdit::editingFinished, this, [this]() {
        const QString next = AsVirtualPath(lineEdit_->text());
        lineEdit_->setText(next);
        if (onChanged_) {
            onChanged_(next);
        }
    });
    QObject::connect(browseButton_, &QPushButton::clicked, this, [this]() { OnBrowse(); });
}

QString PathEditor::path() const {
    return lineEdit_ != nullptr ? lineEdit_->text() : QString{};
}

void PathEditor::setPath(const QString& value) const {
    if (lineEdit_ != nullptr) {
        lineEdit_->setText(AsVirtualPath(value));
    }
}

QString PathEditor::BuildFileFilter() const {
    QStringList extensions;
    const auto raw = customAttributes_.value("FileExtensions");
    if (raw.metaType().id() == QMetaType::QStringList) {
        extensions = raw.toStringList();
    } else if (raw.metaType().id() == QMetaType::QVariantList) {
        const auto list = raw.toList();
        for (const QVariant& item : list) {
            const QString value = item.toString().trimmed();
            if (!value.isEmpty()) {
                extensions.push_back(value);
            }
        }
    }

    QStringList resolved;
    static const QStringList imageExtensions{
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".webp", ".tif", ".tiff", ".dds"
    };
    for (const QString& extension : extensions) {
        QString normalized = extension.trimmed().toLower();
        if (normalized.isEmpty()) {
            continue;
        }
        if (normalized == "image") {
            resolved.append(imageExtensions);
            continue;
        }
        if (!normalized.startsWith('.')) {
            normalized.prepend('.');
        }
        resolved.append(normalized);
    }
    resolved.removeDuplicates();

    if (resolved.isEmpty()) {
        return "All Files (*)";
    }

    QStringList patterns;
    for (const QString& extension : resolved) {
        patterns.push_back(QString("*%1").arg(extension));
    }
    return QString("Allowed Files (%1);;All Files (*)").arg(patterns.join(' '));
}

QString PathEditor::AsVirtualPath(const QString& path) {
    if (path.isEmpty()) {
        return {};
    }

    const QString clean = path.trimmed();
    if (clean.startsWith(Engine::Utils::SourcePath::CORE_PATH_FORMAT) ||
        clean.startsWith(Engine::Utils::SourcePath::LOCAL_PATH_FORMAT)) {
        QString normalized = clean;
        normalized.replace('\\', '/');
        return normalized;
    }

    try {
        return Engine::Utils::SourcePath::OsToCorePath(clean);
    } catch (...) {
    }
    try {
        return Engine::Utils::SourcePath::OsToLocalPath(clean);
    } catch (...) {
    }
    return clean;
}

QString PathEditor::AsOsPath(const QString& path) {
    if (path.isEmpty()) {
        return {};
    }
    return Engine::Utils::SourcePath::ToOsPath(path.trimmed());
}

void PathEditor::OnBrowse() {
    QString currentPath = AsOsPath(path());
    if (currentPath.isEmpty()) {
        currentPath = Engine::Utils::SourcePath::GetSourcePath({});
    }

    QFileInfo info(currentPath);
    QString startDir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (startDir.isEmpty()) {
        startDir = Engine::Utils::SourcePath::GetSourcePath({});
    }

    const QString selectedPath = QFileDialog::getOpenFileName(this, "Select File", startDir, BuildFileFilter());
    if (selectedPath.isEmpty()) {
        return;
    }

    const QString next = AsVirtualPath(selectedPath);
    lineEdit_->setText(next);
    if (onChanged_) {
        onChanged_(next);
    }
}

} // namespace Lvs::Studio::Widgets::Properties

