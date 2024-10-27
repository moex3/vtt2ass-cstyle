#include <stdio.h>
#include <sys/param.h>

#include "reader.h"
#include "tokenizer.h"
#include "parser.h"

#include "cuetext.h"
#include "srt.h"
#include "ass.h"
#include "util.h"
#include "font.h"
#include "opts.h"


#include <locale.h>


#include <unistd.h>
int main(int argc, const char **argv)
{
    setlocale(LC_ALL, "en_US.utf8");

    int en;
    en = opts_parse(argc, argv);
    if (en == -1)
        return 1;

    util_init();
    font_init();

    // TODO: cont. with vertical rendering fixes and vertical ruby

    en = rdr_init(opts_infile);
    if (en != 0)
        return 1;

    struct dyna *tokens = tok_tokenize();
    if (tokens == NULL) {
        printf("Failed to tokenize\n");
        return 2;
    }

    struct dyna *cues = NULL, *styles = NULL;
    en = prs_parse_tokens(tokens, &cues, &styles);
    if (en != 0)
        goto end;

    /* For debug */
#if 0
    printf("Token array size: %ld\n", tokens->e_idx);
    char tokstr[1024];
    for (int i = 0; i < MIN(1000000, tokens->e_idx); i++) {
        tok_2str((struct token*)dyna_elem(tokens, i), 512, tokstr);
        printf("Token %d: %s\n", i, tokstr);
    }

    for (int i = 0; i < cues->e_idx; i++) {
        struct cue *c = dyna_elem(cues, i);
        prs_cue2str(sizeof(tokstr), tokstr, c);
        printf("%s\n\n", tokstr);

        //struct dyna *ctxt_tokens = ctxt_parse(c->text);
    }

    for (int i = 0; styles && i < styles->e_idx; i++) {
        struct cue_style *cs = dyna_elem(styles, i);
        cuestyle_print(sizeof(tokstr), tokstr, cs);
        printf("%s\n", tokstr);
    }
#endif

    if (opts_ass) {
        struct video_info vinf = {
            .width = opts_ass_vid_w,
            .height = opts_ass_vid_h,
        };
        //ass_write(cues, styles, &vinf, "ipaexg.ttf", opts_ass_outfile);
        ass_write(cues, styles, &vinf, opts_ass_fontfile, opts_ass_outfile);
    }
    if (opts_srt) {
        srt_write(cues, styles, opts_srt_outfile);
    }
    printf("Conversion done\n");

end:
    if (cues)
        dyna_destroy(cues);
    if (styles)
        dyna_destroy(styles);
    dyna_destroy(tokens);
    rdr_free();
    font_dinit();
    return 0;
}
