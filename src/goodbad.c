#include "sys.h"
#include "structs.h"
#include "dataset_hdr.h"


chroma_t at_center(raw_state_t* state)
{
	const int chroma_w = FRAME_W >> 1;
	const int i = (FRAME_H >> 1) * chroma_w + (chroma_w >> 1);
	return state->view.chroma[i];
}

int main(int argc, const char* argv[])
{
	PROC_NAME = argv[0];

	dataset_header_t hdr = {};
	raw_example_t ex = {};

	read(0, &hdr, sizeof(hdr));
	
	if(hdr.magic != MAGIC)
	{
		b_log("Incompatible version!");
		return -1;
	}

	write(1, &hdr, sizeof(hdr));
	
	read(0, &ex, sizeof(ex));
	write(1, &ex, sizeof(ex));

	chroma_t c = at_center(&ex.state);
	chroma_t min = c, max = c;

	time_t name = time(NULL);

	while(1)
	{
		read(0, &ex, sizeof(ex));
		ex.state.view.luma[((FRAME_H >> 1) * FRAME_W) + (FRAME_W >> 1)] = 0;

		chroma_t c = at_center(&ex.state);
		int expanded = 0;

		if(c.cr < min.cr)
		{
			min.cr = c.cr;
			expanded = 1;
		}

		if(c.cb < min.cb)
		{
			min.cb = c.cb;
			expanded = 1;
		}

		if(c.cr > max.cr)
		{
			max.cr = c.cr;
			expanded = 1;
		}

		if(c.cb > max.cb)
		{
			max.cb = c.cb;
			expanded = 1;
		}

		if(expanded)
		{
			b_log("cr [ %d - %d ]", min.cr, max.cr);
			b_log("cb [ %d - %d ]", min.cb, max.cb);

			char path[256];
			snprintf(path, sizeof(path), "/var/predictor/color/%s/%ld", argv[0], name);
			b_log("writing to '%s'", path);
			int fd = open(path, O_CREAT | O_WRONLY, 0666);

			if(fd < 0)
			{
				b_log("Error creating '%s'", path);
				return -2;
			}

		       	write(fd, &min, sizeof(min) << 1);
			close(fd);	
		}

		write(1, &ex, sizeof(ex));
	}

	return 0;
}
