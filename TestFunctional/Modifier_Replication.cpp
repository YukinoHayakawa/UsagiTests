#include <Usagi/Modules/Common/Functional/Modifiers/SystemReplicateFillEntities.hpp>

#include "../TestECSCommon/App.hpp"

using namespace usagi;

namespace
{
struct CReplicationOrigin
{
    EntityId id;
};

struct CExampleValue
{
    int value;
};

using SysReplicateValue = SystemReplicateFillEntities<
    CQuery<C<CReplicationOrigin>>,
    CReplicationOrigin,
    C<CExampleValue>
>;

struct Runtime { };

using EntityReplicationTest = TestApp<
    C<CReplicationOrigin, CExampleValue>,
    Runtime,
    SysReplicateValue
>;
}

TEST_F(EntityReplicationTest, ReplicateFillEntitiesTest)
{
    Archetype<CExampleValue> archetype_origin;

    archetype_origin(C<CExampleValue>()).value = 1;
    const auto id0 = db.insert(archetype_origin);
    archetype_origin(C<CExampleValue>()).value = 2;
    [[maybe_unused]]
    const auto id1 = db.insert(archetype_origin);
    archetype_origin(C<CExampleValue>()).value = 3;
    const auto id2 = db.insert(archetype_origin);

    Archetype<CReplicationOrigin> archetype_dest;
    archetype_dest(C<CReplicationOrigin>()).id = id0;
    db.insert(archetype_dest);
    archetype_dest(C<CReplicationOrigin>()).id = id2;
    db.insert(archetype_dest);

    for(auto &&e : filtered_range(CQuery<C<CReplicationOrigin>>()))
    {
        EXPECT_FALSE(e.include(C<CExampleValue>()));
    }

    update_system<SysReplicateValue>();

    auto range = filtered_range(CQuery<C<CReplicationOrigin>>());
    auto iter = range.begin();

    ASSERT_TRUE((*iter).include(C<CExampleValue>()));
    EXPECT_EQ((*iter)(C<CExampleValue>()).value, 1);
    ++iter;
    ASSERT_TRUE((*iter).include(C<CExampleValue>()));
    EXPECT_EQ((*iter)(C<CExampleValue>()).value, 3);
    ++iter;

    EXPECT_EQ(iter, range.end());
}
