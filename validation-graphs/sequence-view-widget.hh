#ifndef SEQUENCE_VIEW_WIDGET_HH
#define SEQUENCE_VIEW_WIDGET_HH

#include <gtk/gtk.h>
#include <mmdb2/mmdb_manager.h>

#if __cplusplus > 201402L
    // good times
    #include <memory>
#else
    // cope
    #include <memory>
    // Copied from: https://gist.github.com/chinmaygarde/970fd5bbd124754b7d36
    // Thank you kind man
    namespace std {
        template <typename T, typename... Args>
        unique_ptr<T> make_unique(Args&&... args) {
            return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        }
    }
#endif

G_BEGIN_DECLS

#define COOT_SEQUENCE_VIEW_TYPE (coot_sequence_view_get_type ())
G_DECLARE_FINAL_TYPE  (CootSequenceView, coot_sequence_view, COOT, COOT_SEQUENCE_VIEW, GtkWidget)

CootSequenceView *coot_sequence_view_new();

G_END_DECLS

void coot_sequence_view_set_structure(CootSequenceView* self, mmdb::Manager *mol);


#endif // SEQUENCE_VIEW_WIDGET_HH
