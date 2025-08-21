#include "unity.h"
#include "unity_fixture.h"

#include "qspi.h"


TEST_GROUP(QSPI_Functional);

TEST_SETUP(QSPI_Functional)
{
}

TEST_TEAR_DOWN(QSPI_Functional)
{
}

TEST(QSPI_Functional, pass_test)
{
    TEST_ASSERT_EQUAL(1, 1); // This will pass
}
TEST(QSPI_Functional, fail_test)
{
    TEST_ASSERT_EQUAL(1, 1); // This will fail
}

TEST(QSPI_Functional, QSPI_IsInitialized_before_initcall)
{
    TEST_ASSERT_FALSE(QSPI_IsInitialized());
}
