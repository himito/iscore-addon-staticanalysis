#include <StaticAnalysis/ScenarioVisitor.hpp>
#include "iscore_plugin_staticanalysis.hpp"

namespace iscore {

}  // namespace iscore

iscore_addon_staticanalysis::iscore_addon_staticanalysis() :
    QObject {}
{
}

iscore::GUIApplicationPlugin* iscore_addon_staticanalysis::make_guiApplicationPlugin(
        const iscore::GUIApplicationContext& app)
{
    return new stal::ApplicationPlugin{app};
}
