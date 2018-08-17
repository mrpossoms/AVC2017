#include "sys.h"
#include <stdio.h>


void timegate_open(timegate_t* tg)
{
	gettimeofday(&tg->start, NULL);
}


long diff_us(struct timeval then, struct timeval now)
{
	long us = (now.tv_sec - then.tv_sec) * 1e6;

	if(us)
	{
		us = (us - then.tv_usec) + now.tv_usec;
	}
	else
	{
		us = now.tv_usec - then.tv_usec;
	}

	return us;
}


void timegate_close(timegate_t* tg)
{
	struct timeval now = {};
	gettimeofday(&now, NULL);

	long residual = diff_us(tg->start, now);
	residual = tg->interval_us - residual;

	if(residual < 0) return;
	usleep(residual);
}


int calib_load(const char* path, calib_t* cal)
{
	int cal_fd = open(ACTION_CAL_PATH, O_RDONLY);

	if(cal_fd < 0)
	{
		return -1;
	}

	if(read(cal_fd, cal, sizeof(calib_t)) != sizeof(calib_t))
	{
		return -2;
	}

	close(cal_fd);
	return 0;
}

const char* PROC_NAME;
void b_log(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, (size_t)sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, "[%s] %s (%d)\n", PROC_NAME, buf, errno);
}


void b_good(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, (size_t)sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, AVC_TERM_GREEN "[%s]" AVC_TERM_COLOR_OFF " %s (%d)\n", PROC_NAME, buf, errno);
}


void b_bad(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, (size_t)sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, AVC_TERM_RED "[%s]" AVC_TERM_COLOR_OFF " %s (%d)\n", PROC_NAME, buf, errno);
}

void yuv422_to_rgb(uint8_t* luma, chroma_t* uv, color_t* rgb, int w, int h)
{
	for(int yi = h; yi--;)
	for(int xi = w; xi--;)
	{
		int i = yi * w + xi;
		int j = yi * (w >> 1) + (xi >> 1);

		rgb[i].r = clamp(luma[i] + 1.14 * (uv[j].cb - 128));
		rgb[i].g = clamp(luma[i] - 0.395 * (uv[j].cr - 128) - (0.581 * (uv[j].cb - 128)));
		rgb[i].b = clamp(luma[i] + 2.033 * (uv[j].cr - 128));
	}
}


float clamp(float v)
{
	v = v > 255 ? 255 : v;
	return  v < 0 ? 0 : v;
}


int path_exists(const char* path)
{
	struct stat path_stat;
	if (stat(path, &path_stat))
	{
		b_bad("Couldn't access '%s'", path);
		return 0;
	}

	return 1;
}


void cli_help(char* const argv[], const char* prog_desc, const char* cmds, const char* cmd_desc[])
{
	int cmd_idx = 0;
	printf("%s\n", argv[0]);
	printf("%s\n", (prog_desc));
	for (int i = 0; i < strlen((cmds)); i++)
	{
		const char* desc = (cmd_desc)[cmd_idx];
		if ((cmds)[i] == ':') continue;
		if (cmds[i] == 'h') desc = "Show this help";
		printf("-%c\t%s\n", (cmds)[i], desc);
		cmd_idx++;
	}
	exit(0);
}


int cli(
	const char* prog_desc,
	cli_cmd_t cmds[],
	int argc,
	char* const argv[]
)
{
	int res = 0, num_cmds = 0;
	for (; cmds[num_cmds].flag; num_cmds++);

	// allocate and build the argument string
	char* arg_str = (char*)calloc((num_cmds + 1) * 2, sizeof(char));
	if (!arg_str) { res = -1; goto done; } // escape on allocation failure
	for(int s = 0, i = num_cmds; i--;)
	{
		const char* fstr = cmds[i].opts.has_value ? "%c:" : "%c";
		s += sprintf(arg_str + s, fstr, cmds[i].flag);
	}

	arg_str[strlen(arg_str)] = 'h';

	// process
	int c;
	while ((c = getopt(argc, argv, arg_str)) != -1)
	for (int i = num_cmds; i--;)
	{
		cli_cmd_t* cmd = cmds + i;

		if (c == 'h')
		{
			const char* cmd_descs[num_cmds];
			for (int i = num_cmds; i--;) { cmd_descs[(num_cmds - 1) - i] = cmds[i].desc; }
			cli_help(argv, prog_desc, arg_str, cmd_descs);
		}
		else if (cmd->flag == c)
		{
			cmd->_present = 1;
			switch(cmd->type)
			{
				case ARG_TYP_FLAG:
					*((int*)cmd->set) = 1;
					break;
				case ARG_TYP_INT:
					*((int*)cmd->set) = atoi(optarg);
					break;
				case ARG_TYP_STR:
				{
					char** str = (char**)cmd->set;
					int len = strlen(optarg);
					*str = (char*)calloc(len + 1, sizeof(char));
					if (!*str)
					{
						res = -(10 + i);
						goto done;
					}
					strncpy(*str, optarg, len);
				} break;
				case ARG_TYP_CALLBACK:
					res = ((int(*)(char,const char*))cmd->set)(c, optarg);
					if (res) { goto done; }
					break;
			}
		}
	}

	// check that all required options are fulfilled
	for (int i = num_cmds; i--;)
	{
		cli_cmd_t* cmd = cmds + i;

		if (cmd->opts.required && !cmd->_present)
		{
			b_bad("Missing required %s -%c for %s",
				cmd->opts.has_value ? "parameter" : "flag",
				cmd->flag,
				cmd->desc
			);
			res = -2;
		}
	}

done:
	free(arg_str);
	return res;
}


int write_pipeline_payload(message_t* msg)
{
	size_t expected_size = sizeof(dataset_hdr_t);
	if (!msg) return -1;

	if (write(1, &msg->header, expected_size) != expected_size)
	{
		return -2;
	}

	switch (msg->header.type)
	{
		case PAYLOAD_ACTION:
			expected_size = sizeof(raw_action_t);
			break;
		case PAYLOAD_STATE:
			expected_size = sizeof(raw_state_t);
			break;
		case PAYLOAD_PAIR:
			expected_size = sizeof(raw_state_t) + sizeof(raw_action_t);
			break;
	}

	if (write(1, &msg->payload, expected_size) != expected_size)
	{
		return -3;
	}

	return 0;
}


int read_pipeline_payload(message_t* msg, payload_type_t exp_type)
{
	size_t expected_size = sizeof(dataset_hdr_t);
	if (!msg) return -1;

	if (read(0, &msg->header, expected_size) != expected_size)
	{
		return -2;
	}

	if (msg->header.magic != MAGIC)
	{
		b_bad(
			"Incorrect magic number got: %lx expected %lx",
			msg->header.magic,
			MAGIC
		);
		return -4;
	}

	if (!(msg->header.type & exp_type))
	{
		b_bad("Incompatible msg type %lx, expected %lx", msg->header.type, exp_type);
		return -5;
	}

	switch (msg->header.type)
	{
		case PAYLOAD_ACTION:
			expected_size = sizeof(raw_action_t);
			break;
		case PAYLOAD_STATE:
			expected_size = sizeof(raw_state_t);
			break;
		case PAYLOAD_PAIR:
			expected_size = sizeof(raw_action_t) + sizeof(raw_state_t);
			break;
	}

	size_t needed = expected_size;
	off_t  off = 0;
	uint8_t* buf = (uint8_t*)&msg->payload;

	while(needed)
	{
		size_t gotten = read(0, buf + off, needed);
		needed -= gotten;
		off += gotten;
	}

	return 0;
}
