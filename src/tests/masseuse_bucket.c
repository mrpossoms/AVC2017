#include "curves.h"
#include "test.h"


int bucket_test(void)
{
	int res = 0;
	long m = random() % 100;
	long M = (random() % 100) + m;
	long count = random() % (M - m);

	if(count == 0)
	{
		count = 1;
	}

	range_t range = {
		.min = m,
		.max = M
	};

	Log("Min: %f, Max: %f\n", 1, range.min, range.max);
	Log("Range: [%d - %d], buckets: %d\n", 1, m, M, count);

	int value = (random() % count) + m;
	int bucket = count * (value-m) / (M - m);
	int actual_bucket = bucket_index(value, &range, count);

	Log("Value: %d\n", 1, value);
	Log("Predicted bucket: %d, actual bucket: %d", 1, bucket, actual_bucket);

	if(actual_bucket != bucket)
	{
		return -1;
	}

	float mu = actual_bucket % 7;
	count = 7;

	for(int i = count; i--;)
	{
		int j = falloff(mu, i) * 20;

		write(1, "|", 1);
		for(; j--;)
		{
			write(1, "#", 1);
		}
		write(1, "\n", 1);
	}

	return 0;
}

TEST_BEGIN
	.name = "Curves bucket test",
	.description = "",
	.run = bucket_test,
TEST_END
