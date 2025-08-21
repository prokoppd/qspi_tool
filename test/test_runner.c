#include "unity_fixture.h"


TEST_GROUP_RUNNER(QSPI_Functional)
{
    RUN_TEST_CASE(QSPI_Functional, pass_test);
    RUN_TEST_CASE(QSPI_Functional, fail_test);
    RUN_TEST_CASE(QSPI_Functional, QSPI_IsInitialized_before_initcall);
}
