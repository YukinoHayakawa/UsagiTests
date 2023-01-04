#include <gtest/gtest.h>

#include <Usagi/Entity/Component.hpp>
#include <Usagi/Modules/Common/Functional/Algorithms/OperatorConstantValue.hpp>
#include <Usagi/Modules/Common/Functional/Algorithms/OperatorCountEntities.hpp>
#include <Usagi/Modules/Common/Functional/Modifiers/SystemMaintainConstantEntityCount.hpp>

#include "../TestECSCommon/App.hpp"

using namespace usagi;

namespace 
{
USAGI_DECL_TAG_COMPONENT(CDeadTag);
struct CExampleVal
{
    int val;
};
}

struct Runtime { };

using ArchetypeT = Archetype<CExampleVal>;

struct Initializer
{
    int i = 0;

    // todo: whether to allow the initializer to access db?
    void operator()(ArchetypeT &archetype, auto &&rt, auto &&db)
    {
        archetype(C<CExampleVal>()).val = i++;
    }
};

using SysMaintainAliveEntityCount = SystemMaintainConstantEntityCount<
    CQuery<C<CExampleVal>, C<CDeadTag>>,
    OperatorConstantValue<std::size_t, 12>,
    ArchetypeT,
    Initializer
>;

using TestMaintainEntityCount = TestApp<
    C<CDeadTag, CExampleVal>,
    Runtime,
    SysMaintainAliveEntityCount
>;

TEST_F(TestMaintainEntityCount, Test)
{
    using CountTotal = OperatorCountEntities<CQuery<C<CExampleVal>>>;
    using CountAlive = OperatorCountEntities<CQuery<C<CExampleVal>, C<CDeadTag>>>;
    using CountDead = OperatorCountEntities<CQuery<C<CExampleVal, CDeadTag>>>;

    auto db_access = db.create_access<ComponentAccessAllowAll>();

    EXPECT_EQ(CountTotal()(runtime, db_access), 0);

    // should spawn 12 alive entities
    update_system<SysMaintainAliveEntityCount>();

    EXPECT_EQ(CountTotal()(runtime, db_access), 12);
    EXPECT_EQ(CountAlive()(runtime, db_access), 12);
    EXPECT_EQ(CountDead()(runtime, db_access), 0);

    auto range = db_access.view(C<CExampleVal>());

    EXPECT_EQ(std::distance(range.begin(), range.end()), 12);

    for(int i = 0; auto &&e: range)
    {
        EXPECT_EQ(e(C<CExampleVal>()).val, i++);
        // kill the first two entities
        if(i < 3) e.add_component(C<CDeadTag>());
    }

    EXPECT_EQ(CountTotal()(runtime, db_access), 12);
    EXPECT_EQ(CountAlive()(runtime, db_access), 10);
    EXPECT_EQ(CountDead()(runtime, db_access), 2);

    // should spawn another two to replace the 
    update_system<SysMaintainAliveEntityCount>();
    
    EXPECT_EQ(CountTotal()(runtime, db_access), 14);
    EXPECT_EQ(CountAlive()(runtime, db_access), 12);
    EXPECT_EQ(CountDead()(runtime, db_access), 2);
}
