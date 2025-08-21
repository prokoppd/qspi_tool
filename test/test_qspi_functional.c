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

TEST(QSPI_Functional, QSPI_IsInitialized_before_initcall)
{
    TEST_ASSERT_FALSE(QSPI_IsInitialized());
}
