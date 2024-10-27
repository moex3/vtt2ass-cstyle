#include "opts.h"

#include <argparse.h>
#include <stdio.h>
#include <string.h>

static const char *const usage[] = {
    "v2a ass [-h] srt [-h] input_file",
    NULL,
};
static const char *const ass_usage[] = {
    "v2a ass [ass options] input_file...",
    NULL,
};
static const char *const srt_usage[] = {
    "v2a srt [srt options] input_file...",
    NULL,
};

bool opts_srt = false;
bool opts_ass = false;
const char *opts_ass_outfile = NULL;
const char *opts_srt_outfile = NULL;
const char *opts_infile = NULL;
int opts_ass_vid_w = 0, opts_ass_vid_h = 0;
const char *opts_ass_fontfile = NULL;
bool opts_ass_debug_boxes = false;
int opts_ass_border_size = -1;

static int cmd_ass(int *argc, const char **argv)
{
    char *outpath = NULL, *fontfile = NULL;
    int width = 0, height = 0, border = -1;
    bool debug = false;

    struct argparse argp;
    struct argparse_option opts[] = {
        OPT_HELP(),
        OPT_STRING('o', "output", &outpath, "output file", NULL, 0, 0),
        OPT_INTEGER('W', "width", &width, "Width of the video file", NULL, 0, 0),
        OPT_INTEGER('H', "height", &height, "Height of the video file", NULL, 0, 0),
        OPT_STRING('f', "font", &fontfile, "The fontfile to use. This font should be embedded in the .mkv", NULL, 0, 0),
        OPT_INTEGER('B', "border", &border, "Set the border size to use", NULL, 0, 0),
        OPT_BOOLEAN('D', "debug", &debug, "If set, debug boxes will be included in the output", NULL, 0, 0),
        OPT_END(),
    };
    argparse_init(&argp, opts, ass_usage, ARGPARSE_STOP_AT_NON_OPTION);
    *argc = argparse_parse(&argp, *argc, argv);

    if (outpath == NULL) {
        printf("Output file option is required\n");
        argparse_usage(&argp);
        return -1;
    }
    if (width == 0 || height == 0) {
        printf("Video width and height is required\n");
        argparse_usage(&argp);
        return -1;
    }
    if (fontfile == NULL) {
        printf("The font file option is required\n");
        argparse_usage(&argp);
        return -1;
    }

    opts_ass = true;
    opts_ass_outfile = outpath;
    opts_ass_vid_w = width;
    opts_ass_vid_h = height;
    opts_ass_fontfile = fontfile;
    opts_ass_debug_boxes = debug;
    opts_ass_border_size = border;
    return 0;
}

static int cmd_srt(int *argc, const char **argv)
{
    char *outpath = NULL;

    struct argparse argp;
    struct argparse_option opts[] = {
        OPT_HELP(),
        OPT_STRING('o', "output", &outpath, "output file", NULL, 0, 0),
        OPT_END(),
    };
    argparse_init(&argp, opts, srt_usage, ARGPARSE_STOP_AT_NON_OPTION);
    *argc = argparse_parse(&argp, *argc, argv);

    if (outpath == NULL) {
        printf("Output file option is required\n");
        argparse_usage(&argp);
        return -1;
    }
    opts_srt = true;
    opts_srt_outfile = outpath;
    return 0;
}

int opts_parse(int argc, const char **argv)
{
    struct argparse argp;
    struct argparse_option opts[] = {
        OPT_HELP(),
        OPT_END(),
    };
    int r = argparse_init(&argp, opts, usage, ARGPARSE_STOP_AT_NON_OPTION);

    argc = argparse_parse(&argp, argc, argv);
    if (argc < 1) {
        argparse_usage(&argp);
        return -1;
    }

    const char *filepath = NULL;
    const char *subcname = argv[0];

    while (true) {
        if (strcmp(subcname, "ass") == 0) {
            r = cmd_ass(&argc, argv);
        } else if (strcmp(subcname, "srt") == 0) {
            r = cmd_srt(&argc, argv);
        } else {
            if (argc < 1)
                break;
            if (argc > 1) {
                subcname = argv[0];
                continue;
            }
            filepath = argv[0];
            break;
        }
        if (r != 0)
            return r;
        if (argc < 1)
            break;
        subcname = argv[0];
    }

    if (filepath == NULL) {
        printf("Input file is not given\n");
        return -1;
    }

    if (!opts_ass && !opts_srt) {
        printf("At least ass or srt conversaton needs to be specified\n");
        return -1;
    }

    opts_infile = filepath;
    return 0;
}
