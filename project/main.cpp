#include "imageStitcher.hpp"

int main()
{
	QString query = qgetenv("QUERY_STRING");
	if (query.isEmpty())
		query =
		  "103.7448400&1.2341900&1&36&4&/data/GroupObjects/36/stitchedKFC/"
		  "4.tif&/data/GroupObjects/36/satellitePhoto/"
		  "5_1.tif&/data/GroupObjects/36/satellitePhoto/5_2.tif";

	ImageStitcher img_s;

	img_s.run(query.split("&"));

	return 0;
}
