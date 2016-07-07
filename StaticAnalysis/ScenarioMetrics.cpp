#include "ScenarioMetrics.hpp"
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <iscore/tools/std/Algorithms.hpp>
namespace stal
{
template<class>
class LanguageVisitor;

/*
    // How to count "default" values ?
    // Operateurs : association processus - contrainte et état - contrainte et état-evenement et état - timenode

    /// Quand. On créé une contrainte dans le vide : auto-completion d'etats de fin

    // Exposer structure d'arbre du scénario en js?
    // Cela permettrait de coder un visiteur super simplement pour calculer les valeurs de halstead

    // Faire parallèle entre data flow et time flow ? dans data flow, cable est un opérateur

    // Opérateurs:  after, of, duration, expression, {}, [;], constraint, state, event, timenode, scenario, loop, automation
    // ex.
    //
    // constraint c1 after s0
    // duration [2, oo] of c1

    // scenario sx {
    // ...
    // } of c1

    // state s2 after c1
    // state s2 of e1
    // event e1 of t1
    // state s0 of e0
    // event e0 of t0
    // timenode t0
    // expression {(tze < 123)} of e0
    // scenario sx of c1
    */
template<>
class LanguageVisitor<Scenario::ProcessModel>
{
    public:
        const Scenario::ProcessModel& m_scenar;
        QString text;

        LanguageVisitor(const Scenario::ProcessModel& scenar):
            m_scenar{scenar}
        {
            text += " {"
                    + QString("\n");

            for(const auto& elt : scenar.constraints)
            {
                visit(elt);
            }

            for(const auto& elt : scenar.events)
            {
                visit(elt);
            }

            for(const auto& elt : scenar.timeNodes)
            {
                visit(elt);
            }

            for(const auto& elt : scenar.states)
            {
                visit(elt);
            }

            text += "}";
        }

        template<typename T>
        QString id(const T& c)
        {
            return QString(Metadata<Description_k, T>::get()) + QString::number(c.id_val());
        }

        QString duration(const Scenario::ConstraintModel& c)
        {
            QString s;
            if(c.duration.isRigid())
            {
                s = QString::number(c.duration.defaultDuration().msec()) + "ms";
            }
            else
            {
                s += "[";
                s += QString::number(c.duration.minDuration().msec()) + "; ";
                if(c.duration.maxDuration().isInfinite())
                {
                    s += "oo";
                }
                else
                {
                    s += QString::number(c.duration.maxDuration().msec());
                }
                s += "]";
            }
            return s;
        }

        void visit(const Scenario::ConstraintModel& c)
        {
            text += "constraint "   + id(c)
                    + " after " + id(startState(c, m_scenar))
                    + QString("\n");
            text += "duration " + duration(c)
                    + " of " + id(c)
                    + QString("\n");

            for(auto& process : c.processes)
            {
                if(auto scenar = dynamic_cast<Scenario::ProcessModel*>(&process))
                {
                    text += "scenario " + id(*scenar)
                            + LanguageVisitor<Scenario::ProcessModel>{*scenar}.text
                            + " of " + id(c)
                            + QString("\n");
                }
            }
        }

        void visit(const Scenario::EventModel& e)
        {
            text += "event " + id(e)
                    + " of " + id(parentTimeNode(e, m_scenar))
                    + QString("\n");


            if(e.condition().childCount() > 0)
            {
                text += "expression '" + e.condition().toString()
                        + "' of " + id(e)
                        + QString("\n");
            }
        }

        void visit(const Scenario::TimeNodeModel& tn)
        {
            text += "timenode " + id(tn)
                    + QString("\n");

            if(tn.trigger() && tn.trigger()->expression().childCount() > 0)
            {
                text += "expression '" + tn.trigger()->expression().toString()
                        + "' of " + id(tn)
                        + QString("\n");
            }
        }

        void visit(const Scenario::StateModel& st)
        {
            if(st.previousConstraint())
            {
                text += "state " + id(st)
                        + " after " + id(previousConstraint(st, m_scenar))
                        + QString("\n");
            }

            text += "state " + id(st)
                    + " of " + id(parentEvent(st, m_scenar))
                    + QString("\n");
        }
};

struct ScenarioFactors
{
        // Operators
        struct operators_t {
                int scenario{};

                int constraint{};
                int event{};
                int state{};
                int timenode{};

                int of{};
                int after{};
                int lbrace{};
                int rbrace{};

                int expression{};
                int duration{};

                std::vector<int> toVector() const
                {
                    return {scenario, constraint, event, state, timenode, of, after, lbrace, rbrace, expression, duration};
                }

        } operators;

        // Operands
        struct operands_t {
                QMap<QString, int> variables;
                std::vector<QMap<QString, int>> subprocesses_variables;
                int expressions{};
                int constraint_rigid_times{};
                int constraint_minmax_times{};


                std::vector<int> toVector() const
                {
                    std::vector<int> v;

                    for(auto elt : variables)
                    {
                        v.push_back(elt);
                    }

                    for(auto vec : subprocesses_variables)
                    {
                        for(auto elt : vec)
                        {
                            v.push_back(elt);
                        }
                    }

                    v.push_back(expressions);
                    v.push_back(constraint_rigid_times);
                    v.push_back(constraint_minmax_times);

                    return v;
                }

        } operands;

        ScenarioFactors& operator+=(const ScenarioFactors& other)
        {
            operators.scenario += other.operators.scenario;

            operators.constraint += other.operators.constraint;
            operators.event += other.operators.event;
            operators.state += other.operators.state;
            operators.timenode += other.operators.timenode;

            operators.of += other.operators.of;
            operators.after += other.operators.after;
            operators.lbrace += other.operators.lbrace;
            operators.rbrace += other.operators.rbrace;

            operators.expression += other.operators.expression;
            operators.duration += other.operators.duration;

            operands.subprocesses_variables.push_back(other.operands.variables);
            copy(other.operands.subprocesses_variables, operands.subprocesses_variables);

            operands.expressions += other.operands.expressions;
            operands.constraint_rigid_times += other.operands.constraint_rigid_times;
            operands.constraint_minmax_times += other.operands.constraint_minmax_times;

            return *this;
            // TODO -Werror=return-type
        }
};

static int sum_unique(const std::vector<int>& vec)
{
    int val = 0;
    for(int e : vec)
    {
        val += int(e > 0);
    }
    return val;
}

static int sum_all(const std::vector<int>& vec)
{
    return std::accumulate(vec.begin(), vec.end(), 0);
}

template<class>
class HalsteadVisitor;
template<>
class HalsteadVisitor<Scenario::ProcessModel>
{
    public:
        const Scenario::ProcessModel& m_scenar;
        ScenarioFactors f;

        HalsteadVisitor(const Scenario::ProcessModel& scenar):
            m_scenar{scenar}
        {
            f.operators.lbrace += 1;

            for(const auto& elt : scenar.constraints)
            { visit(elt); }

            for(const auto& elt : scenar.events)
            { visit(elt); }

            for(const auto& elt : scenar.timeNodes)
            { visit(elt); }

            for(const auto& elt : scenar.states)
            { visit(elt); }

            f.operators.rbrace += 1;
        }

        template<typename T>
        QString id(const T& c)
        {
            return QString(Metadata<Description_k, T>::get()) + QString::number(c.id_val());
        }

        void duration(const Scenario::ConstraintModel& c)
        {
            if(c.duration.isRigid())
            {
                f.operands.constraint_rigid_times += 1;
            }
            else
            {
                f.operands.constraint_minmax_times += 1;
            }
        }

        void visit(const Scenario::ConstraintModel& c)
        {
            f.operators.constraint += 1;
            f.operands.variables[id(c)] += 1;
            f.operators.after += 1;
            f.operands.variables[id(startState(c, m_scenar))] += 1;

            f.operators.duration += 1;
            f.operators.of += 1;
            f.operands.variables[id(c)] += 1;

            for(auto& process : c.processes)
            {
                if(auto scenar = dynamic_cast<Scenario::ProcessModel*>(&process))
                {
                    f.operators.scenario += 1;
                    f.operands.variables[id(*scenar)] += 1;
                    f += HalsteadVisitor<Scenario::ProcessModel>{*scenar}.f;
                    f.operators.of += 1;
                    f.operands.variables[id(c)] += 1;
                }
            }
        }

        void visit(const Scenario::EventModel& e)
        {
            f.operators.event += 1;
            f.operands.variables[id(e)] += 1;
            f.operators.of += 1;
            f.operands.variables[id(parentTimeNode(e, m_scenar))] += 1;

            if(e.condition().childCount() > 0)
            {
                f.operators.expression += 1;
                f.operands.expressions += 1;
                f.operators.of += 1;
                f.operands.variables[id(e)] += 1;
            }
        }

        void visit(const Scenario::TimeNodeModel& tn)
        {
            f.operators.timenode += 1;
            f.operands.variables[id(tn)] += 1;

            if(tn.trigger() && tn.trigger()->expression().childCount() > 0)
            {
                f.operators.expression += 1;
                f.operands.expressions += 1;
                f.operators.of += 1;
                f.operands.variables[id(tn)] += 1;
            }
        }

        void visit(const Scenario::StateModel& st)
        {
            if(st.previousConstraint())
            {
                f.operators.state += 1;
                f.operands.variables[id(st)] += 1;
                f.operators.after += 1;
                f.operands.variables[id(previousConstraint(st, m_scenar))] += 1;
            }

            f.operators.state += 1;
            f.operands.variables[id(st)] += 1;
            f.operators.of += 1;
            f.operands.variables[id(parentEvent(st, m_scenar))] += 1;
        }
};

QString stal::Metrics::toScenarioLanguage(
        const Scenario::ProcessModel& s)
{
    return LanguageVisitor<Scenario::ProcessModel>{s}.text;
}

stal::Metrics::Halstead::Factors
stal::Metrics::Halstead::ComputeFactors(const Scenario::ProcessModel& scenar)
{
    auto sf = HalsteadVisitor<Scenario::ProcessModel>{scenar}.f;
    stal::Metrics::Halstead::Factors factors;
    factors.eta1 = sum_unique(sf.operators.toVector());
    factors.eta2 = sum_unique(sf.operands.toVector());
    factors.N1 = sum_all(sf.operators.toVector());
    factors.N2 = sum_all(sf.operands.toVector());
    return factors;
}




// construction du CFG :
// 1. on liste tous les programmes
// i.e. les bouts de code indépendants
// 2. pour chaque entrée, on cherche les blocs "simples" (qui n'ont pas de conditions) :
// on va de chaque contrainte au timenode le plus lointain qui n'a pas de trigger / condition.
// -> algorithme de flot avec marquage.



/*
template<typename Scenario_T>
auto ConstraintsAttachedToStartInSameBlock(
        const ConstraintModel& cst,
        const Scenario_T& scenario)
{
    // We make a list of events in the same block.
    // It's the event on the parent timenode of the start of the constraint
    // with no conditions.
    //std::vector<Id<EventModel>> events_of_previous_block;
    std::vector<Id<EventModel>> events_of_next_block;

    // We at least return the next constraints on the start event.
    std::vector<Id<ConstraintModel>> constraints;
    const EventModel& previous_event = startEvent(cst, scenario);
    events_of_next_block.push_back(previous_event.id());

    const TimeNodeModel& tn = parentTimeNode(previous_event, scenario);
    // If there is a trigger on the time node, we don't take into account
    // all the previous elements.
    bool trigger_exists = tn.trigger()->expression().hasChildren();

    // First check if there is a condition on the previous event.
    if(!previous_event.condition().hasChildren())
    {

        // We can add the previous constraints.
//        if(!trigger_exists)
//        {
//            events_of_previous_block.push_back(previous_event.id());
//        }


        // Check if there is a condition on the events in the same timenode.
        for(const auto& event_id: tn.events())
        {
            if(event_id != previous_event.id())
            {
                const EventModel& other_event = scenario.events.at(event_id);
                if(!other_event.condition().hasChildren())
                {
                    events_of_next_block.push_back(event_id);
//                    if(!trigger_exists)
//                    {
//                        events_of_previous_block.push_back(event_id);
//                    }
                }
            }
        }
    }

    // Now, for each previous & next block event,
    // we add the constraints.
    for(auto& event_id : events_of_next_block)
    {
        const auto& ev = scenario.events.at(event_id);
        for(auto& constraint : nextConstraints(ev, scenario))
        {
            if(constraint != cst.id())
            {
                constraints.push_back(constraint);
            }
        }
    }

//    for(auto& event_id : events_of_previous_block)
//    {
//        const auto& ev = scenario.events.at(event_id);
//        for(auto& constraint : previousConstraints(ev, scenario))
//        {
//            constraints.push_back(constraint);
//        }
//    }

    return constraints;
}

struct MarkedConstraints
{
    using Mark = int;
    Mark currentMark = 0;
    static const constexpr int NoMark = -1;
    const Scenario::ProcessModel& m_scenar;
    MarkedConstraints(const Scenario::ProcessModel& scenar):
        m_scenar{scenar}
    {
        for(const auto& cst : scenar.constraints)
        {
            marks.insert(std::make_pair(cst.id(), NoMark));
        }

        marking();
    }

    void mark_recursive_previous(const Id<ConstraintModel>& id, Mark mark)
    {
        auto& cst = m_scenar.constraints.at(id);
        for(const auto& c_id : ConstraintsAttachedToStartInSameBlock(cst, m_scenar))
        {
            marks.at(cst.id()) = mark;
            mark_recursive_previous(c_id, mark);
            mark_recursive_next(c_id, mark);
        }
    }

    void mark_recursive_next(const Id<ConstraintModel>& id, Mark mark)
    {

        auto& cst = m_scenar.constraints.at(id);
        for(const auto& c_id : ConstraintsAttachedToEndInSameBlock(cst, m_scenar))
        {
            marks.at(cst.id()) = mark;
            mark_recursive_previous(c_id, mark);
            mark_recursive_next(c_id, mark);
        }
    }

    void marking()
    {
        for(const auto& event : m_scenar.events)
        {
        }








        for(const auto& cst : m_scenar.constraints)
        {
            // We start from constraints that don't have previous constraints.

            if(marks.at(cst.id()) == NoMark)
            {
                // First we choose a mark
                int mark = currentMark;
                marks.at(cst.id()) = mark;
                mark_recursive_previous(cst.id(), mark);
                mark_recursive_next(cst.id(), mark);
                currentMark++;
            }
        }
    }

    std::map<Id<ConstraintModel>, int> marks;
};
*/

struct Program
{
        std::vector<Id<Scenario::ConstraintModel>> constraints;
        std::vector<Id<Scenario::EventModel>> events;
        std::vector<Id<Scenario::TimeNodeModel>> nodes;
        std::vector<Id<Scenario::StateModel>> states;
};

using Mark = int;
static const constexpr int NoMark = -1;
// A program is a connected set of constraints, etc.
class ProgramVisitor
{
        Mark currentMark = 0;

        std::map<Id<Scenario::ConstraintModel>, Mark> constraints;
        std::map<Id<Scenario::EventModel>, Mark> events;
        std::map<Id<Scenario::TimeNodeModel>, Mark> nodes;
        std::map<Id<Scenario::StateModel>, Mark> states;

        const Scenario::ProcessModel& m_scenar;
    public:
        ProgramVisitor(const Scenario::ProcessModel& scenar):
            m_scenar{scenar}
        {
            for(const auto& elt : scenar.constraints)
            {
                constraints.insert(std::make_pair(elt.id(), NoMark));
            }
            for(const auto& elt : scenar.timeNodes)
            {
                nodes.insert(std::make_pair(elt.id(), NoMark));
            }
            for(const auto& elt : scenar.events)
            {
                events.insert(std::make_pair(elt.id(), NoMark));
            }
            for(const auto& elt : scenar.states)
            {
                states.insert(std::make_pair(elt.id(), NoMark));
            }

            // Take a constraint and recursively mark everything
            for(const auto& constraint : scenar.constraints)
            {
                if(constraints.at(constraint.id()) == NoMark)
                {
                    mark(constraint.id(), currentMark);
                    currentMark++;
                }
            }
        }

        std::vector<Program> programs()
        {
            std::vector<Program> prog;
            prog.reserve(currentMark);
            for(Mark m = 0; m < currentMark; m++)
            {
                Program p;

                for(const auto& elt : constraints)
                {
                    if(elt.second == m)
                    {
                        p.constraints.push_back(elt.first);
                    }
                }

                for(const auto& elt : events)
                {
                    if(elt.second == m)
                    {
                        p.events.push_back(elt.first);
                    }
                }

                for(const auto& elt : nodes)
                {
                    if(elt.second == m)
                    {
                        p.nodes.push_back(elt.first);
                    }
                }

                for(const auto& elt : states)
                {
                    if(elt.second == m)
                    {
                        p.states.push_back(elt.first);
                    }
                }

                prog.push_back(std::move(p));
            }

            return prog;
        }

    private:
        void mark(const Id<Scenario::ConstraintModel>& cid, Mark m)
        {
            ISCORE_ASSERT(constraints.at(cid) == m || constraints.at(cid) == NoMark);
            if(constraints.at(cid) == m)
            {
                return;
            }

            constraints.at(cid) = m;

            auto& cst = m_scenar.constraints.at(cid);
            states.at(startState(cst, m_scenar).id()) = m;
            states.at(endState(cst, m_scenar).id()) = m;
            events.at(startEvent(cst, m_scenar).id()) = m;
            events.at(endEvent(cst, m_scenar).id()) = m;
            nodes.at(startTimeNode(cst, m_scenar).id()) = m;
            nodes.at(endTimeNode(cst, m_scenar).id()) = m;

            // Mark all that's before the start node.
            auto& start_node = startTimeNode(cst, m_scenar);
            for(const auto& cst : previousConstraints(start_node, m_scenar))
            {
                mark(cst, m);
            }

            // Mark all that's after the start node.
            for(const auto& cst : nextConstraints(start_node, m_scenar))
            {
                mark(cst, m);
            }

            // Mark all that's before the end node.
            auto& end_node = endTimeNode(cst, m_scenar);
            for(const auto& cst : previousConstraints(end_node, m_scenar))
            {
                mark(cst, m);
            }

            // Mark all that's after the end node.
            for(const auto& cst : nextConstraints(end_node, m_scenar))
            {
                mark(cst, m);
            }
        }

};

static auto startingTimeNodes(const Program& program, const Scenario::ProcessModel& scenario)
{
    std::list<Id<Scenario::TimeNodeModel>> startingNodes;
    for(const auto& node_id : program.nodes)
    {
        const auto& node = scenario.timeNodes.at(node_id);
        auto csts = previousConstraints(node, scenario);
        if(csts.empty())
        {
            startingNodes.push_back(node_id);
        }
    }
    return startingNodes;
}

// For each starting point we propagate the flow in blocks in the following fashion :
// Each condition yields a new block.
// Each timenode with a trigger yields a new block.
// Each starting point yields a new block.
// If there is a single block before a timenode,
// and if there is no trigger / condition, then the block continues afterwards.

struct BaseBlock
{
        BaseBlock(int b): block{b} {}
        int block{};
        std::vector<Id<Scenario::ConstraintModel>> constraints;
        std::vector<Id<Scenario::EventModel>> events;
        std::vector<Id<Scenario::TimeNodeModel>> nodes;
};

class CyclomaticVisitor
{
        // Here the mark refers to the block id of the group of elements.
        Mark maxMark = 0;

        std::map<Id<Scenario::ConstraintModel>, Mark> constraints;
        std::map<Id<Scenario::EventModel>, Mark> events;
        std::map<Id<Scenario::TimeNodeModel>, Mark> nodes;

    public:
        const Program& m_program;
        const Scenario::ProcessModel& m_scenar;

        CyclomaticVisitor(
                const Program& program,
                const Scenario::ProcessModel& scenar):
            m_program{program},
            m_scenar{scenar}
        {
            using namespace Scenario;

            for(const auto& elt : scenar.constraints)
            {
                constraints.insert(std::make_pair(elt.id(), NoMark));
            }
            for(const auto& elt : scenar.timeNodes)
            {
                nodes.insert(std::make_pair(elt.id(), NoMark));
            }
            for(const auto& elt : scenar.events)
            {
                events.insert(std::make_pair(elt.id(), NoMark));
            }

            std::list<Id<TimeNodeModel>> startingNodes = startingTimeNodes(program, scenar);
            std::list<Id<EventModel>> startingEvents;
            while(!startingNodes.empty() || !startingEvents.empty())
            {
                while(!startingNodes.empty())
                {
                    auto node_id = *startingNodes.begin();
                    auto val = computeNode(node_id, maxMark);

                    std::list<Id<TimeNodeModel>> newNodes(val.first.begin(), val.first.end());
                    startingNodes.splice(startingNodes.end(), newNodes);
                    startingNodes.remove(node_id);

                    std::list<Id<EventModel>> newEvents(val.second.begin(), val.second.end());
                    startingEvents.splice(startingEvents.end(), newEvents);

                    maxMark++;
                }

                while(!startingEvents.empty())
                {
                    auto event_id = *startingEvents.begin();
                    auto val = computeEvent(event_id, maxMark);

                    std::list<Id<TimeNodeModel>> newNodes(val.first.begin(), val.first.end());
                    startingNodes.splice(startingNodes.end(), newNodes);

                    std::list<Id<EventModel>> newEvents(val.second.begin(), val.second.end());
                    startingEvents.splice(startingEvents.end(), newEvents);
                    startingEvents.remove(event_id);

                    maxMark++;
                }
            }
        }

        std::vector<BaseBlock> blocks() const
        {
            std::vector<BaseBlock> blocks;

            for(Mark m = 0; m < maxMark; m++)
            {
                BaseBlock b{m};

                for(const auto& elt : constraints)
                {
                    if(elt.second == m)
                    {
                        b.constraints.push_back(elt.first);
                    }
                }

                for(const auto& elt : events)
                {
                    if(elt.second == m)
                    {
                        b.events.push_back(elt.first);
                    }
                }

                for(const auto& elt : nodes)
                {
                    if(elt.second == m)
                    {
                        b.nodes.push_back(elt.first);
                    }
                }

                blocks.push_back(std::move(b));
            }
            return blocks;
        }
        std::pair<std::set<Id<Scenario::TimeNodeModel>>, std::set<Id<Scenario::EventModel>>> computeEvent(
                const Id<Scenario::EventModel>& event,
                Mark m)
        {
            using namespace Scenario;
            std::set<Id<TimeNodeModel>> notSame,   prev_notSame;
            std::set<Id<TimeNodeModel>> notSure,   prev_notSure;
            std::set<Id<EventModel>> notSameEvent, prev_notSameEvent;

            events.at(event) = m;
            const auto& ev = m_scenar.events.at(event);
            for(const auto& cid : nextConstraints(ev, m_scenar))
            {
                mark(cid, m, notSame, notSure, notSameEvent);
            }
            return iterate(m, notSame, notSure, notSameEvent);
        }

        std::pair<std::set<Id<Scenario::TimeNodeModel>>, std::set<Id<Scenario::EventModel>>> computeNode(
                const Id<Scenario::TimeNodeModel>& node,
                Mark m)
        {
            using namespace Scenario;
            std::set<Id<TimeNodeModel>> notSame;
            std::set<Id<TimeNodeModel>> notSure;
            std::set<Id<EventModel>> notSameEvent;
            mark(node, m, notSame, notSure, notSameEvent);
            return iterate(m, notSame, notSure, notSameEvent);
        }

        std::pair<std::set<Id<Scenario::TimeNodeModel>>, std::set<Id<Scenario::EventModel>>> iterate(
                Mark m,
                std::set<Id<Scenario::TimeNodeModel>>& notSame,
                std::set<Id<Scenario::TimeNodeModel>>& notSure,
                std::set<Id<Scenario::EventModel>>& notSameEvent)
        {
            using namespace Scenario;
            std::set<Id<TimeNodeModel>> prev_notSame;
            std::set<Id<TimeNodeModel>> prev_notSure;
            std::set<Id<EventModel>> prev_notSameEvent;
            do
            {
                // As long as we gain new information, we iterate.
                prev_notSame = notSame;
                prev_notSure = notSure;
                prev_notSameEvent = notSameEvent;

                for(const auto& id : notSure)
                {
                    // We check again since this may have changed
                    auto newState = timeNodeIsInSameBlock(m_scenar.timeNodes.at(id), m);

                    switch(newState)
                    {
                    case NodeInBlock::Same:
                        notSure.erase(id);
                        mark(id, m, notSame, notSure, notSameEvent);
                        break;
                    case NodeInBlock::NotSure:
                        // do nothing
                        break;
                    case NodeInBlock::NotSame:
                        notSame.insert(id);
                        notSure.erase(id);
                        break;
                    }
                }
            } while(prev_notSame != notSame || prev_notSure != notSure || prev_notSameEvent != notSameEvent);

            // Now we should have reached our limits.
            notSame.insert(notSure.begin(), notSure.end());
            notSure.clear();

            return std::make_pair(notSame, notSameEvent);

        }

        // Three levels :
        //  in same block,
        //  not sure (some constraints are unmarked),
        //  not in same block
        enum class NodeInBlock { Same, NotSure, NotSame} ;
        NodeInBlock timeNodeIsInSameBlock(const Scenario::TimeNodeModel& tn, Mark mark)
        {
            // True if no condition,
            // or if previous constraints are from different blocks
            if(tn.trigger()->active())
                return NodeInBlock::NotSame;

            auto prev_csts = previousConstraints(tn, m_scenar);
            if(any_of(prev_csts, [&] (const auto& cst) {
                      auto constraint_mark = constraints.at(cst);
                    return constraint_mark != mark && constraint_mark != NoMark;
                }))
            {
                return NodeInBlock::NotSame;
            }
            else if(any_of(prev_csts, [&] (const auto& cst) {
                           auto constraint_mark = constraints.at(cst);
                         return constraint_mark != mark && constraint_mark == NoMark;
                     }))
            {
                return NodeInBlock::NotSure;
            }
            else
            {
                return NodeInBlock::Same;
            }
        }

        bool eventIsInSameBlock(const Scenario::EventModel& ev)
        {
            return !ev.condition().hasChildren();
        }

        void mark(const Id<Scenario::TimeNodeModel>& id,
                  Mark m,
                  std::set<Id<Scenario::TimeNodeModel>>& notSame,
                  std::set<Id<Scenario::TimeNodeModel>>& notSure,
                  std::set<Id<Scenario::EventModel>>& notSameEvent)
        {
            // We flow for as long as we can and stop once
            // new blocks are encountered
            const auto& tn = m_scenar.timeNodes.at(id);
            nodes.at(id) = m;

            // For each event, if they have a condition, they introduce a new mark
            for(const auto& event_id : tn.events())
            {
                mark(event_id, m, notSame, notSure, notSameEvent);
            }
        }


        void mark(const Id<Scenario::EventModel>& event_id,
                  Mark m,
                  std::set<Id<Scenario::TimeNodeModel>>& notSame,
                  std::set<Id<Scenario::TimeNodeModel>>& notSure,
                  std::set<Id<Scenario::EventModel>>& notSameEvent)
        {
            // We flow for as long as we can and stop once
            // new blocks are encountered
            const auto& ev = m_scenar.events.at(event_id);
            if(eventIsInSameBlock(ev))
            {
                events.at(event_id) = m;
                for(const auto& cid : nextConstraints(ev, m_scenar))
                {
                    mark(cid, m, notSame, notSure, notSameEvent);
                }
            }
            else
            {
                notSameEvent.insert(event_id);
            }

        }

        void mark(const Id<Scenario::ConstraintModel>& id,
                  Mark m,
                  std::set<Id<Scenario::TimeNodeModel>>& notSame,
                  std::set<Id<Scenario::TimeNodeModel>>& notSure,
                  std::set<Id<Scenario::EventModel>>& notSameEvent)
        {
            const auto& cst = m_scenar.constraints.at(id);
            constraints.at(id) = m;

            const auto& endNode = endTimeNode(cst, m_scenar);
            auto sameblock = timeNodeIsInSameBlock(endNode, m);
            switch(sameblock)
            {
            case NodeInBlock::Same:
                mark(endNode.id(), m, notSame, notSure, notSameEvent);
                break;
            case NodeInBlock::NotSure:
                notSure.insert(endNode.id());
                break;
            case NodeInBlock::NotSame:
                notSame.insert(endNode.id());
                break;
            }
        }
};

stal::Metrics::Cyclomatic::Factors
stal::Metrics::Cyclomatic::ComputeFactors(
        const Scenario::ProcessModel& scenar)
{
    ProgramVisitor v(scenar);
    auto programs = v.programs();

    // For each program, we count the number of edge / vertice.

    // First case : each constraint is an edge, each node / event is a vertice.
    int N = std::accumulate(programs.begin(), programs.end(), 0,
                            [] (int size, const Program& program) { return size + program.constraints.size(); });
    int E_events = std::accumulate(programs.begin(), programs.end(), 0,
                                   [] (int size, const Program& program) { return size + program.events.size(); });
    int E_nodes = std::accumulate(programs.begin(), programs.end(), 0,
                                  [] (int size, const Program& program) { return size + program.nodes.size(); });

    return stal::Metrics::Cyclomatic::Factors{E_events + E_nodes, N, (int)programs.size()};
}

stal::Metrics::Cyclomatic::Factors stal::Metrics::Cyclomatic::ComputeFactors2(
        const Scenario::ProcessModel& scenar)
{
    ProgramVisitor v(scenar);
    auto programs = v.programs();

    // Second case, more intelligent.
    int program_n = 0;
    int E = 0;
    int N = 0;
    int P = programs.size();
    for(const auto& program : programs)
    {
        CyclomaticVisitor vis2(program, scenar);
        auto blocks = vis2.blocks();

        for(auto i = 0U; i < blocks.size(); i++)
        {
            const auto& block = blocks[i];
            N += 1;
            std::set<int> nextBlocks;
            // We search all the adjacent forward blocks and we add edges.

            for(const auto& elt_id : block.constraints)
            {
                auto& elt = scenar.constraints.at(elt_id);
                elt.metadata.setLabel(QString::number(program_n) + " - " + QString::number(i));


                auto& tn = endTimeNode(elt, scenar);
                auto it = find_if(blocks, [&] (const BaseBlock& block) {
                   return contains(block.nodes, tn.id());
                });
                if(it != blocks.end())
                {
                    nextBlocks.insert(it->block && it->block != block.block);
                }
            }
            for(const auto& elt_id : block.events)
            {
                auto& elt = scenar.events.at(elt_id);
                elt.metadata.setLabel(QString::number(program_n) + " - " + QString::number(i));
            }
            for(const auto& elt_id : block.nodes)
            {
                auto& elt = scenar.timeNodes.at(elt_id);
                elt.metadata.setLabel(QString::number(program_n) + " - " + QString::number(i));

                for(const auto& event : elt.events())
                {
                    auto it = find_if(blocks, [&] (const BaseBlock& block) {
                        return contains(block.events, event);
                    });
                    if(it != blocks.end() && it->block != block.block)
                    {
                        nextBlocks.insert(it->block);
                        nextBlocks.insert(-it->block); // Twice for the disabled part
                    }
                }
            }

            E += nextBlocks.size();


        }
        program_n++;
    }

    // To compute the metric, we create the graph between blocks.
    // For each block, if it is adjacent to another block,
    // we add an edge between them.
    // If it begins with a condition we create two blocks (one for the "false" case).
    // If it begins with a trigger,
    //  - if the range is infinite :
    //  - else it does not change.

    return  stal::Metrics::Cyclomatic::Factors{E, N, P};
}
}
