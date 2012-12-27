#ifndef __PLAYTREE_H
#define __PLAYTREE_H

#include "osdep/mplib.h"
#include "xmpcore/xmp_enums.h"
#include <vector>
#include <stack>

struct m_config_t;
namespace mpxp {
    struct Stream;
    struct libinput_t;
}

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

namespace mpxp {
    struct play_tree_param_t {
	std::string name;
	std::string value;
    };

    class PlayTree : public Opaque {
	public:
	    PlayTree();
	    virtual ~PlayTree();

	    static PlayTree*	parse_playtree(libinput_t& libinput,Stream* stream);
	    static PlayTree*	parse_playlist_file(libinput_t&libinput,const std::string& file);
	    virtual MPXP_Rc	cleanup();
	    // If childs is true mp_free also the childs
	    virtual void	free(int childs);
	    virtual void	free_list(int childs);

	    virtual void	set_child(PlayTree* child);
	    virtual void	set_parent(PlayTree* parent);
	    // Add at end
	    virtual void	append_entry(PlayTree* entry);
	    // And on begining
	    virtual void	prepend_entry(PlayTree* entry);
	    // Insert after
	    virtual void	insert_entry(PlayTree* entry);
	    // Detach from the tree
	    virtual void	remove(int free_it,int with_childs);
	    virtual void	add_file(const std::string& file);
	    virtual MPXP_Rc	remove_file(const std::string& file);
	    virtual void	set_param(const std::string& name,const std::string& val);
	    virtual MPXP_Rc	unset_param(const std::string& name);
	    /// Copy the config parameters from one item to another.
	    virtual void	set_params_from(const PlayTree& src);
	    virtual void	set_flag(int flags, int deep);
	    virtual void	unset_flag(int flags, int deep);
	    virtual MPXP_Rc	is_valid() const;

	    PlayTree*		get_parent() const { return parent; }
	    PlayTree*		get_child() const { return child; }
	    PlayTree*		get_prev() const { return prev; }
	    PlayTree*		get_next() const { return next; }
	    void		set_prev(PlayTree* p) { prev=p; }
	    void		set_next(PlayTree* p) { next=p; }
	    const std::string&	get_file(size_t idx) const { return files[idx]; }
	    const std::vector<std::string>&	get_files() const { return files; }
	    const play_tree_param_t&		get_param(size_t idx) const { return params[idx]; }
	    const std::vector<play_tree_param_t>&get_params() const { return params; }
	    int			get_entry_type() const { return entry_type; }
	    int			get_loop() const { return loop; }
	    void		set_loop(int l) { loop=l; }
	    int			get_flags() const { return flags; }
	    void		set_flags(int f) { flags=f; }
	private:
	    PlayTree* parent;
	    PlayTree* child;
	    PlayTree* next;
	    PlayTree* prev;

	    std::vector<play_tree_param_t> params;
	    int loop;
	    std::vector<std::string> files;
	    int entry_type;
	    int flags;
    };

    struct _PlayTree_Iter : public Opaque {
	public:
	    _PlayTree_Iter(PlayTree* parent,m_config_t& config);
	    _PlayTree_Iter(const _PlayTree_Iter& old);
	    virtual ~_PlayTree_Iter();

	    // d is the direction : d > 0 == next , d < 0 == prev
	    // with_node : TRUE == stop on nodes with childs, FALSE == go directly to the next child
	    virtual int		step(int d,int with_nodes);
	    // Break a loop, etc
	    virtual int		up_step(int d,int with_nodes);
	    // Enter a node child list
	    virtual int		down_step(int d,int with_nodes);
	    virtual std::string	get_file(int d);

	    PlayTree*		get_root() const { return root; }
	    PlayTree*		get_tree() const { return tree; }
	    void		set_tree(PlayTree* _tree) { tree=_tree; }
	    void		reset_tree() { tree=root; }
	    int			get_file() const { return file; }
	    int			get_num_files() const { return num_files; }
	private:
	    void		push_params();
	    PlayTree*		root; // Iter root tree
	    PlayTree*		tree; // Current tree
	    m_config_t&		config;
	    int			loop;  // Looping status
	    int			file;
	    int			num_files;
	    int			entry_pushed;
	    int			mode;

	    std::stack<int> status_stack;
    };
} // namespace mpxp

/// \defgroup PtAPI Playtree highlevel API
/// \ingroup Playtree
/// Highlevel API with pt-suffix to different from low-level API
/// by Fabian Franz (mplayer@fabian-franz.de).
///@{

/// Frees the iter.
void pt_iter_destroy(_PlayTree_Iter** iter);

/// Gets the next available file in the direction (d=-1 || d=+1).
std::string pt_iter_get_file(_PlayTree_Iter* iter, int d);

// Two Macros that implement forward and backward direction.
static inline std::string pt_iter_get_next_file(_PlayTree_Iter* iter) { return pt_iter_get_file(iter, 1); }
static inline std::string pt_iter_get_prev_file(_PlayTree_Iter* iter) { return pt_iter_get_file(iter, -1); }

/// Inserts entry into the playtree.
void pt_iter_insert_entry(_PlayTree_Iter* iter, PlayTree* entry);

/// Replaces current entry in playtree with entry by doing insert and remove.
void pt_iter_replace_entry(_PlayTree_Iter* iter, PlayTree* entry);

/// Adds a new file to the playtree, if it is not valid it is created.
void pt_add_file(PlayTree** ppt,const std::string& filename);

/// \brief Performs a convert to playtree-syntax, by concat path/file
/// and performs pt_add_file
void pt_add_gui_file(PlayTree** ppt,const std::string& path,const std::string& file);

// Two macros to use only the iter and not the other things.
static inline void pt_iter_add_file(_PlayTree_Iter* iter, const std::string& filename) { PlayTree* tree=iter->get_tree();  pt_add_file(&tree, filename); }
static inline void pt_iter_add_gui_file(_PlayTree_Iter* iter,const std::string& path,const std::string& name) { PlayTree* tree=iter->get_tree(); pt_add_gui_file(&tree, path, name); }

/// Resets the iter and goes back to head.
void pt_iter_goto_head(_PlayTree_Iter* iter);

///@}

#endif
