//
// Created by samuel on 10/9/24.
//

#include <gtest/gtest.h>

#include "ecs/HiveECS.h"

class ECSTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		hive::ecs::ECS::init();
	}
	void TearDown() override
	{
		hive::ecs::ECS::shutdown();
	}
};
TEST_F(ECSTest, ShouldCreateEntity)
{
	auto entity = hive::ecs::ECS::createEntity();
	EXPECT_NE(&entity, nullptr);
}

TEST_F(ECSTest, ShouldAddComponent)
{
	auto entity = hive::ecs::ECS::createEntity();

	struct Player{};
	hive::ecs::ECS::addComponent<Player>(entity);

	EXPECT_TRUE(hive::ecs::ECS::hasComponent<Player>(entity));
}


class TestSystem : public hive::ecs::System
{
public:
	void init() override
	{
		is_init = true;
	}

	void update(float deltaTime) override
	{
		is_updated = true;
	}

	bool is_init = false;
	bool is_updated = false;

};
TEST_F(ECSTest, SystemShouldInitWhenRegister)
{
	TestSystem *test_system = new TestSystem();
	hive::ecs::ECS::registerSystem(test_system, "TestSystem");

	EXPECT_TRUE(test_system->is_init);
}

TEST_F(ECSTest, SystemShouldUpdate)
{
	TestSystem *test_system = new TestSystem();

	hive::ecs::ECS::registerSystem(test_system, "TestSystem");

	hive::ecs::ECS::updateSystems(0);

	EXPECT_TRUE(test_system->is_updated);
}

TEST_F(ECSTest, SystemShouldBeRemoved)
{
	TestSystem *test_system = new TestSystem();

	hive::ecs::ECS::registerSystem(test_system, "TestSystem");
	hive::ecs::ECS::removeSystem("TestSystem");

	hive::ecs::ECS::updateSystems(0);

	EXPECT_TRUE(test_system->is_init);
	EXPECT_FALSE(test_system->is_updated);
}

TEST_F(ECSTest, SystemShouldNotUpdateIfDisabled)
{
	TestSystem *test_system = new TestSystem();

	hive::ecs::ECS::registerSystem(test_system, "TestSystem");

	test_system->m_is_active = false;

	hive::ecs::ECS::updateSystems(0);

	EXPECT_TRUE(test_system->is_init);
	EXPECT_FALSE(test_system->is_updated);
}


TEST_F(ECSTest, QueryBuilderShouldFetchCorrectEntity)
{

	auto entity = hive::ecs::ECS::createEntity();
	auto entity2 = hive::ecs::ECS::createEntity();

	struct Player
	{
		int id;
	};

	struct Enemy
	{

	};
	hive::ecs::ECS::addComponent<Player>(entity, 20);
	hive::ecs::ECS::addComponent<Player>(entity2, 33);

	auto query = hive::ecs::QueryBuilder<Player>();

	auto it = query.viewEach().begin();
	auto [e1, p1] = *it;
	it.operator++();

	auto [e2, p2] = *it;


	EXPECT_TRUE(p1.id == 33);
	EXPECT_TRUE(p2.id == 20);
}


TEST_F(ECSTest, QueryBuilderShouldFetchWithMultipleComponent)
{

	auto entity = hive::ecs::ECS::createEntity();
	auto entity2 = hive::ecs::ECS::createEntity();

	struct Player
	{
		int id;
	};

	struct Enemy
	{

	};
	hive::ecs::ECS::addComponent<Player>(entity, 20);
	hive::ecs::ECS::addComponent<Player>(entity2, 33);
	hive::ecs::ECS::addComponent<Enemy>(entity2);

	auto query = hive::ecs::QueryBuilder<Player, Enemy>();

	auto it = query.viewEach().begin();
	auto [e1, p1] = *it;
	it.operator++();

	EXPECT_EQ(it, query.viewEach().end());
	EXPECT_TRUE(p1.id == 33);
}
