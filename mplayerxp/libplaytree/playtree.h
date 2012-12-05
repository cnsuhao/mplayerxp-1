#ifndef __PLAYTREE_H
#define __PLAYTREE_H
#include "libmpstream/stream.h"

enum {
    PLAY_TREE_ITER_ERROR=0,
    PLAY_TREE_ITER_ENTRY=1,
    PLAY_TREE_ITER_NODE =2,
    PLAY_TREE_ITER_END  =3
};
enum {
    PLAY_TREE_ENTRY_NODE=-1,
    PLAY_TREE_ENTRY_DVD =0,
    PLAY_TREE_ENTRY_VCD =1,
    PLAY_TREE_ENTRY_TV  =2,
    PLAY_TREE_ENTRY_FILE=3
};
/// \defgroup PlaytreeEntryFlags Playtree flags
/// \ingroup Playtree
///@{
/// Play the item children in random order.
enum {
    PLAY_TREE_RND	=(1<<0),
/// Playtree flags used by the iterator to mark items already "randomly" played.
    PLAY_TREE_RND_PLAYED=(1<<8),
///@}
/// \defgroup PlaytreeIterMode Playtree iterator mode
/// \ingroup PlaytreeIter
///@{
    PLAY_TREE_ITER_NORMAL=0,
    PLAY_TREE_ITER_RND	 =1
///@}
};
/// \defgroup Playtree
///@{

typedef struct play_tree play_tree_t;
typedef struct play_tree_iter play_tree_iter_t;
typedef struct play_tree_param play_tree_param_t;

#include "libmpconf/cfgparser.h"

#if 0
typedef struct play_tree_info play_tree_info_t;
// TODO : a attrib,val pair system and not something hardcoded
struct play_tree_info {
  char* title;
  char* author;
  char* copyright;
  char* abstract;
  // Some more ??
}
#endif

struct play_tree_param {
  char* name;
  char* value;
};

struct play_tree {
  play_tree_t* parent;
  play_tree_t* child;
  play_tree_t* next;
  play_tree_t* prev;

  //play_tree_info_t info;
  play_tree_param_t* params;
  int loop;
  char** files;
  int entry_type;
  int flags;
};

struct play_tree_iter {
  play_tree_t* root; // Iter root tree
  play_tree_t* tree; // Current tree
  m_config_t* config;
  int loop;  // Looping status
  int file;
  int num_files;
  int entry_pushed;
  int mode;

  int* status_stack; //  loop/valid stack to save/revert status when we go up/down
  int stack_size;  // status stack size
};

play_tree_t* play_tree_new(void);

// If childs is true mp_free also the childs
void
play_tree_free(play_tree_t* pt, int childs);


void
play_tree_free_list(play_tree_t* pt, int childs);


// Childs
void
play_tree_set_child(play_tree_t* pt, play_tree_t* child);
// Or parent
void
play_tree_set_parent(play_tree_t* pt, play_tree_t* parent);


// Add at end
void
play_tree_append_entry(play_tree_t* pt, play_tree_t* entry);

// And on begining
void
play_tree_prepend_entry(play_tree_t* pt, play_tree_t* entry);

// Insert after
void
play_tree_insert_entry(play_tree_t* pt, play_tree_t* entry);

// Detach from the tree
void
play_tree_remove(play_tree_t* pt, int free_it,int with_childs);


void
play_tree_add_file(play_tree_t* pt,const char* file);

int
play_tree_remove_file(play_tree_t* pt,const char* file);


// Val can be NULL
void
play_tree_set_param(play_tree_t* pt,const char* name,const char* val);

int
play_tree_unset_param(play_tree_t* pt,const char* name);
/// Copy the config parameters from one item to another.
void
play_tree_set_params_from(play_tree_t* dest,const play_tree_t* src);


/// Iterator

play_tree_iter_t*
play_tree_iter_new(play_tree_t* pt, m_config_t* config);

play_tree_iter_t*
play_tree_iter_new_copy(play_tree_iter_t const* old);

void
play_tree_iter_free(play_tree_iter_t* iter);

// d is the direction : d > 0 == next , d < 0 == prev
// with_node : TRUE == stop on nodes with childs, FALSE == go directly to the next child

int
play_tree_iter_step(play_tree_iter_t* iter, int d,int with_nodes);

int // Break a loop, etc
play_tree_iter_up_step(play_tree_iter_t* iter, int d,int with_nodes);

int // Enter a node child list
play_tree_iter_down_step(play_tree_iter_t* iter, int d,int with_nodes);

char* play_tree_iter_get_file(play_tree_iter_t* iter, int d);

play_tree_t* parse_playtree(libinput_t* libinput,stream_t * stream);

play_tree_t* play_tree_cleanup(play_tree_t* pt);

play_tree_t* parse_playlist_file(libinput_t*libinput,const char* file);

/// \defgroup PtAPI Playtree highlevel API
/// \ingroup Playtree
/// Highlevel API with pt-suffix to different from low-level API
/// by Fabian Franz (mplayer@fabian-franz.de).
///@{

// Cleans up pt and creates a new iter.
play_tree_iter_t* pt_iter_create(play_tree_t** pt, struct m_config* config);

/// Frees the iter.
void pt_iter_destroy(play_tree_iter_t** iter);

/// Gets the next available file in the direction (d=-1 || d=+1).
char* pt_iter_get_file(play_tree_iter_t* iter, int d);

// Two Macros that implement forward and backward direction.
static inline char* pt_iter_get_next_file(play_tree_iter_t* iter) { return pt_iter_get_file(iter, 1); }
static inline char* pt_iter_get_prev_file(play_tree_iter_t* iter) { return pt_iter_get_file(iter, -1); }

/// Inserts entry into the playtree.
void pt_iter_insert_entry(play_tree_iter_t* iter, play_tree_t* entry);

/// Replaces current entry in playtree with entry by doing insert and remove.
void pt_iter_replace_entry(play_tree_iter_t* iter, play_tree_t* entry);

/// Adds a new file to the playtree, if it is not valid it is created.
void pt_add_file(play_tree_t** ppt,const char* filename);

/// \brief Performs a convert to playtree-syntax, by concat path/file
/// and performs pt_add_file
void pt_add_gui_file(play_tree_t** ppt,const char* path,const char* file);

// Two macros to use only the iter and not the other things.
static inline void pt_iter_add_file(play_tree_iter_t* iter, const char *filename) { pt_add_file(&iter->tree, filename); }
static inline void pt_iter_add_gui_file(play_tree_iter_t* iter,const char* path,const char* name) { pt_add_gui_file(&iter->tree, path, name); }

/// Resets the iter and goes back to head.
void pt_iter_goto_head(play_tree_iter_t* iter);

///@}

#endif
