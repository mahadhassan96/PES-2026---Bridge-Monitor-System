#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

ZTEST(my_suite, test_example)
{
    printk("test running\n");
    zassert_true(1);
}

ZTEST_SUITE(my_suite, NULL, NULL, NULL, NULL, NULL);