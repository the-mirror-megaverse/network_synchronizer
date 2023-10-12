#include "tests.h"

#include "local_network.h"
#include "modules/network_synchronizer/data_buffer.h"
#include "test_processor.h"
#include "test_scene_synchronizer.h"

void NS_Test::test_all() {
	// TODO test DataBuffer.
	test_processor();
	test_local_network();
	test_scene_synchronizer();

	DataBuffer db;
	db.begin_write(0);

	std::string abc_1("abc_1");
	db.add(abc_1);

	db.begin_read();
	std::string abc_1_r;
	db.read(abc_1_r);

	CRASH_COND(abc_1 != abc_1_r);
}