#include "markdownrenderer.h"

#include <cstdlib>

#ifdef HAS_CMARK
extern "C" {
#include <cmark.h>
}
#endif

namespace FeatherNotes {
namespace MarkdownRenderer {

QString normalizeCommonMark (const QString &md)
{
#ifdef HAS_CMARK
    const QByteArray utf8 = md.toUtf8();
    cmark_node *doc = cmark_parse_document (utf8.constData(), static_cast<size_t> (utf8.size()), CMARK_OPT_VALIDATE_UTF8);
    if (!doc)
        return md;

    char *out = cmark_render_commonmark (doc, CMARK_OPT_VALIDATE_UTF8, 0);
    cmark_node_free (doc);
    if (!out)
        return md;

    const QString result = QString::fromUtf8 (out);
    free (out);
    return result;
#else
    return md;
#endif
}

}
}
