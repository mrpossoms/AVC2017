#include "curves.h"
#include "test.h"


// y              = 1 / (1 + e^-x)
// 1 / y          = 1 + e^-x
// 1 / y - 1      = e^-x
// -ln(1 / y - 1) = x

int falloff_test(void)
{
	int res = 0;

	res += NEAR(falloff(0, 0), 0.5f);
	res += NEAR(falloff(0, -1.0986), 0.25f);
	res += NEAR(falloff(0, 1.0986), 0.75f);

	return 0;
}

TEST_BEGIN
	.name = "Curves falloff test",
	.description = "",
	.run = falloff_test,
TEST_END
