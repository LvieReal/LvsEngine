#include "Lvs/Studio/Widgets/Output/OutputDock.hpp"

#include <QPlainTextEdit>

namespace Lvs::Studio::Widgets::Output {

OutputDock::OutputDock(QWidget* parent)
    : QDockWidget("Output", parent) {
    auto* text = new QPlainTextEdit(this);
    text->setReadOnly(true);
    text->setPlainText("Output (prototype)");
    setWidget(text);
}

void OutputDock::BindToPlace(const std::shared_ptr<Lvs::Engine::DataModel::Place>& place) {
    static_cast<void>(place);
}

void OutputDock::Unbind() {
}

} // namespace Lvs::Studio::Widgets::Output
