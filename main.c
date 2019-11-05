#include <alt_types.h>
#include <altera_avalon_performance_counter.h>
#include <altera_avalon_pio_regs.h>
#include <sys/alt_irq.h>
#include <system.h>
#include <stdio.h>
#define SAMPLE_SIZE 7999
#define TEST_ITERATIONS 100

int is_palindrome_unrolled(int *data, int len) {
	int i = 0;
	int j = len - 1;
	while (j - i >= 8) {
		if (data[i] != data[j]) {
			return 0;
		}
		if (data[i + 1] != data[j - 1]) {
			return 0;
		}
		if (data[i + 2] != data[j - 2]) {
			return 0;
		}
		if (data[i + 3] != data[j - 3]) {
			return 0;
		}
		i += 4;
		j -= 4;
	}
	while (i < j) {
		if (data[i] != data[j]) {
			return 0;
		}
		++i;
		--j;
	}
	return 1;
}

int is_palindrome_base(int *data, int len) {
	int i = 0;
	int j = len - 1;
	while (i < j) {
		if (data[i] != data[j]) {
			return 0;
		}
		++i;
		--j;
	}
	return 1;
}

int measure_performance_base(int *sample_data, int len) {
	PERF_BEGIN(PERFORMANCE_COUNTER_0_BASE, 1);
	int is_palindrome_t1 = is_palindrome_base(sample_data, SAMPLE_SIZE);
	PERF_END(PERFORMANCE_COUNTER_0_BASE, 1);
	return is_palindrome_t1;
}

int measure_performance_unrolled(int *sample_data, int len) {
	PERF_BEGIN(PERFORMANCE_COUNTER_0_BASE, 2);
	int is_palindrome_t1 = is_palindrome_unrolled(sample_data, SAMPLE_SIZE);
	PERF_END(PERFORMANCE_COUNTER_0_BASE, 2);
	return is_palindrome_t1;
}

typedef int (*palindrome_checker)(int*, int);

void test_is_palindrome(palindrome_checker checker) {

	int all_the_same[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	int all_the_same_expected = 1;
	int all_the_same_result = checker(all_the_same,
			sizeof(all_the_same) / sizeof(int));
	printf("\ntest all_the_same, expected: %d, returned: %d\n",
			all_the_same_expected, all_the_same_result);

	int palindrome_even[] = { 1, 2, 3, 4, 5, 5, 4, 3, 2, 1 };
	int palindrome_even_expected = 1;
	int palindrome_even_result = checker(palindrome_even,
			sizeof(palindrome_even) / sizeof(int));
	printf("\ntest palindrome_even, expected: %d, returned: %d\n",
			palindrome_even_expected, palindrome_even_result);

	int palindrome_odd[] = { 1, 2, 3, 2, 1 };
	int palindrome_odd_expected = 1;
	int palindrome_odd_result = checker(palindrome_odd,
			sizeof(palindrome_odd) / sizeof(int));
	printf("\ntest palindrome_even, expected: %d, returned: %d\n",
			palindrome_odd_expected, palindrome_odd_result);

	int single[] = { 1 };
	int single_expected = 1;
	int single_result = checker(single, 1);
	printf("\ntest single, expected: %d, returned: %d\n", single_expected,
			single_result);

	int not_palindrome_even[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	int not_palindrome_even_expected = 0;
	int not_palindrome_even_result = checker(not_palindrome_even,
			sizeof(not_palindrome_even) / sizeof(int));
	printf("\ntest not_palindrome_even, expected: %d, returned: %d\n",
			not_palindrome_even_expected, not_palindrome_even_result);

	int not_palindrome_odd[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	int not_palindrome_odd_expected = 0;
	int not_palindrome_odd_result = checker(not_palindrome_odd,
			sizeof(not_palindrome_odd) / sizeof(int));
	printf("\ntest not_palindrome_odd, expected: %d, returned: %d\n",
			not_palindrome_odd_expected, not_palindrome_odd_result);

	int not_palindrome_last_comp[] = { 1, 2, 3, 4, 5, 5, 3, 2, 1 };
	int not_palindrome_last_comp_expected = 0;
	int not_palindrome_last_comp_result = checker(not_palindrome_last_comp,
			sizeof(not_palindrome_last_comp) / sizeof(int));
	printf("\ntest not_palindrome_last_comp, expected: %d, returned: %d\n",
			not_palindrome_last_comp_expected, not_palindrome_last_comp_result);
}

int main() {
	printf("\ntest is_palindrom_base\n ");
	test_is_palindrome(&is_palindrome_base);

	printf("\ntest is_palindrom_unrolled\n ");
	test_is_palindrome(&is_palindrome_unrolled);

	int sample_data[SAMPLE_SIZE];
	for (int i = 0; i < SAMPLE_SIZE; ++i) {
		sample_data[i] = 1;
	}

	PERF_RESET(PERFORMANCE_COUNTER_0_BASE);
	PERF_START_MEASURING(PERFORMANCE_COUNTER_0_BASE);

	int x = 0, y = 0;
	for (int i = 0; i < TEST_ITERATIONS; ++i) {
		x += measure_performance_base(sample_data, SAMPLE_SIZE);
	}
	for (int i = 0; i < TEST_ITERATIONS; ++i) {
		y += measure_performance_unrolled(sample_data, SAMPLE_SIZE);
	}
	printf("base measures: %d, unrolled measures: %d\n", x, y);

	PERF_STOP_MEASURING(PERFORMANCE_COUNTER_0_BASE);
	perf_print_formatted_report(PERFORMANCE_COUNTER_0_BASE,
	ALT_CPU_FREQ, 2, "Base performance", "Unrolled performance");
	return 0;
}

