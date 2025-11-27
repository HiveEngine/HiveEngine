#pragma once

namespace larvae
{
    class TestFixture
    {
    public:
        virtual ~TestFixture() = default;

        virtual void SetUp() {}
        virtual void TearDown() {}

        static void SetUpTestSuite() {}
        static void TearDownTestSuite() {}
    };
}
