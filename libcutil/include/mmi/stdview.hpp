#ifndef STDVIEW_HPP
#define STDVIEW_HPP

/** @file stdview.hpp
 * 
 *  Standard element editor (using schemas)
 * 
 * (C) J A 2008-2009 */

#include "../journal.hpp"
#include "modele.hpp"
#include "cutil.hpp"
#include "mmi/gtkutil.hpp"

namespace utils
{
/** @brief Generic view (MMI) generation */
namespace mmi
{

using namespace model;

class Controleur
{
public:
  // Get dynamic content at the specified path in the view model
  virtual std::string genere_contenu_dynamique(const std::string &action_path,
                                      Node node,
                                      Gtk::Widget **widget) {*widget = nullptr; return "";}
  virtual void gere_action(const std::string &action, Node node) = 0;
};

class VueGenerique;

class FabriqueWidget
{
public:
  virtual VueGenerique *fabrique(Node modele_donnees, Node modele_vue, Controleur *controleur) = 0;
};

class VueGenerique
{
public:
  static VueGenerique *fabrique(Node modele_donnees, Node modele_vue, Controleur *controleur = nullptr);

  static int enregistre_widget(std::string id, FabriqueWidget *frabrique);

  virtual Gtk::Widget *get_gtk_widget() = 0;
  VueGenerique();
  virtual ~VueGenerique();

  // Demande de regénération récursive du contenu dynamique (géré par l'utilisateur)
  virtual void maj_contenu_dynamique();

  virtual void set_sensitive(bool sensitive);

  typedef enum widget_type_enum
  {
    /** Automatic widget type determination according to the model schema */
    WIDGET_AUTO = 0,
    WIDGET_NULL,
    WIDGET_FIELD,
    WIDGET_FIELD_LIST,
    WIDGET_FIXED_STRING,
    WIDGET_BUTTON,
    WIDGET_BORDER_LAYOUT,
    WIDGET_GRID_LAYOUT,
    WIDGET_FIXED_LAYOUT,
    WIDGET_TRIG_LAYOUT,
    WIDGET_NOTEBOOK,
    WIDGET_PANNEAU, // ?
    WIDGET_IMAGE,
    WIDGET_LABEL,
    WIDGET_COMBO,
    WIDGET_DECIMAL_ENTRY,
    WIDGET_HEXA_ENTRY,
    WIDGET_FLOAT_ENTRY,
    WIDGET_RADIO,
    WIDGET_SWITCH,
    WIDGET_CADRE,
    WIDGET_CHOICE,
    WIDGET_BYTES,
    WIDGET_TEXT,
    WIDGET_STRING,
    WIDGET_COLOR,
    WIDGET_DATE,
    WIDGET_FOLDER,
    WIDGET_FILE,
    WIDGET_SERIAL,
    WIDGET_LIST_LAYOUT,
    WIDGET_VBOX,
    WIDGET_HBOX,
    WIDGET_BUTTON_BOX,
    WIDGET_VUE_SPECIALE,
    WIDGET_SEP_H,
    WIDGET_SEP_V,
    WIDGET_TABLE,
    WIDGET_LED,
    WIDGET_INVALIDE
  } WidgetType;

  static const unsigned int WIDGET_MAX = ((int) WIDGET_INVALIDE);

  static WidgetType  desc_vers_type(const std::string &s);
  static std::string type_vers_desc(WidgetType type);

  Node modele_donnees;
  Node modele_vue;
private:
protected:
  std::vector<VueGenerique *> enfants;
  bool is_sensitive;
  Controleur *controleur;
};


class SubPlot: public VueGenerique
{
public:
  SubPlot(Node &data_model, Node &view_model, Controleur *controler);
  Gtk::Widget *get_gtk_widget();
private:
  Gtk::Grid grille;
};

class TrigLayout: public VueGenerique
{
public:
  TrigLayout(Node &data_model, Node &view_model);
  Gtk::Widget *get_gtk_widget();
private:
  Gtk::VBox vbox;
  Gtk::HPaned hpane;
  Gtk::VSeparator vsep;
};


class VueCadre: public VueGenerique
{
public:
  VueCadre(Node &data_model, Node &view_model, Controleur *controler);
  Gtk::Widget *get_gtk_widget();
  ~VueCadre();
private:
  Gtk::Frame cadre;
};


class VueLineaire: public VueGenerique
{
public:
  VueLineaire(int vertical, Node &data_model, Node &view_model, Controleur *controler);
  Gtk::Widget *get_gtk_widget();
  ~VueLineaire();
private:
  int vertical;
  Gtk::Box *box;
  Gtk::VBox vbox;
  Gtk::HBox hbox;
};

class NoteBookLayout: public VueGenerique
{
public:
  NoteBookLayout(Node &data_model, Node &view_model, Controleur *controler);
  Gtk::Widget *get_gtk_widget();
  ~NoteBookLayout();
private:
  Gtk::Notebook notebook;
};


class HButtonBox: public VueGenerique
{
public:
  HButtonBox(Node &data_model, Node &view_model, Controleur *controler = nullptr);
  Gtk::Widget *get_gtk_widget();
private:
  utils::model::Node data_model, view_model;
  Controleur *controler;
  Gtk::HButtonBox hbox;
  struct Action
  {
    std::string name;
    Gtk::Button *button;
    bool need_sel;
    bool is_default;
  };
  std::vector<Action> actions;

  void on_button(std::string action);
};

class CustomWidget: public VueGenerique
{
public:
  CustomWidget(Node &data_model, Node &view_model, Controleur *controler = nullptr);
  Gtk::Widget *get_gtk_widget();
private:
  // Demande de regénération récursive du contenu dynamique (géré par l'utilisateur)
  void maj_contenu_dynamique();
  Node data_model, view_model;
  Controleur *controler;
  std::string id;
  Gtk::EventBox evt_box;
  Gtk::Widget *widget;
};

class ListLayout:
    public VueGenerique,
    private CListener<ChangeEvent>
{
public:
  ListLayout(Node &data_model, Node &view_model, Controleur *controler = nullptr);
  ~ListLayout();
  Gtk::Widget *get_gtk_widget();
private:

  void maj_contenu_dynamique();

  void on_event(const ChangeEvent &ce);

  Node get_selection();
  void rebuild_view();
  void update_view();
  void on_button(std::string action);
  void on_click(const LabelClick &path);

  int current_selection;

  Controleur *controler;
  Gtk::VBox vbox, vbox_ext;
  Gtk::Table table;
  Gtk::VSeparator vsep;
  Gtk::HButtonBox hbox;
  struct Elem
  {
    Node model;
    int id;
    Gtk::VBox *vbox;
    Gtk::Widget *widget;
    SensitiveLabel *label;
    Gtk::Frame *frame;
    Gtk::Alignment *align, *align2;
    Gtk::EventBox *evt_box;
  };
  bool on_button_press_event(GdkEventButton *evt, Elem *elt);
  std::vector<Elem> elems;
  struct Action
  {
    std::string name;
    Gtk::Button *button;
    bool need_sel;
    bool is_default;
  };
  std::vector<Action> actions;
  Gtk::ScrolledWindow scroll;
};


class NodeViewConfiguration
{
public:
  NodeViewConfiguration();
  /** Display attribute descriptions */
  bool show_desc;
  /** Display element descriptions */
  bool show_main_desc;
  /** Display children in a tree view */
  bool show_children;
  /** Small strings fields */
  bool small_strings;
  /** Display a separator between attributes */
  bool show_separator;
  /** Number of columns */
  int nb_columns;

  /** Disable view of all children, even if optionnals, etc. */
  bool disable_all_children;

  /** Display only a tree view, without a view to edit contents */
  bool display_only_tree;

  bool expand_all;

  int  nb_attributes_by_column;

  /** Horizontally split internal view if needed (otherwise vertically) */
  bool horizontal_splitting;

  int table_width, table_height;

  std::string to_string() const;
};

class KeyPosChangeEvent
{
public:
  std::vector<std::string> vchars;
};

class VueTable
  : private CListener<ChangeEvent>,
    public VueGenerique
{
public:

  struct Config
  {
    bool affiche_boutons;
    Config(){affiche_boutons = true;}
  };

  VueTable(Node &modele_donnees, Node &modele_vue, Controleur *controleur);
  VueTable(Node model, NodeSchema *&sub, const NodeViewConfiguration &cfg);

  void init(Node model, NodeSchema *&sub, const NodeViewConfiguration &cfg, Config &config);

  Gtk::Widget *get_gtk_widget();

private:
  Config config;
  Node model;
  int nb_lignes;
  NodeSchema *schema;
  bool lock;
  SubSchema sub_schema;
  bool can_remove;
  bool is_valid();
  
  void update_langue();
  void update_view();
  void clear_table();
  void on_selection_changed();
  Node get_selected();
  void set_selection(Node sub);
  
  void on_editing_done(Glib::ustring path, Glib::ustring text, std::string col);
  void on_editing_start(Gtk::CellEditable *ed, Glib::ustring path, std::string col);
  void on_b_remove();
  void on_b_up();
  void on_b_down();
  void on_b_add();
  void on_b_command(std::string command);
  
  class ModelColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:
      ModelColumns(){}
      Gtk::TreeModelColumn<Glib::ustring> m_cols[30];
      Gtk::TreeModelColumn<Node>       m_col_ptr;
  };

  class MyTreeView: public Gtk::TreeView
  {
  public:
    MyTreeView(VueTable *parent);
    virtual bool on_button_press_event(GdkEventButton *ev);
  private:
    VueTable *parent;
  };

  Gtk::ScrolledWindow scroll;
  MyTreeView tree_view;
  Glib::RefPtr<Gtk::TreeStore> tree_model;
  ModelColumns columns;
  Gtk::VButtonBox button_box;
  Gtk::Button b_add;
  Gtk::Button b_remove, b_up, b_down;
  Gtk::HBox hbox;

  std::vector<Gtk::CellRenderer *> cell_renderers;
 
  void on_event(const ChangeEvent &ce);

  void maj_cellule(Gtk::TreeModel::Row &trow, unsigned int col, unsigned int row);
  void maj_ligne(Gtk::TreeModel::Row &trow, unsigned int row);
};


class AttributeListView;
class FocusInEvent{};

class AttributeView: private   CListener<ChangeEvent>,
                     public    CProvider<FocusInEvent>,
                     public    CProvider<KeyPosChangeEvent>,
                     public    VueGenerique
{
public:

  static AttributeView *factory(Node model, Node view);

  static AttributeView *build_attribute_view(Attribute *model,
                                             const NodeViewConfiguration &config,
                                             Node parent);
  virtual ~AttributeView();
  virtual unsigned int get_nb_widgets()       = 0;
  virtual Gtk::Widget *get_widget(int index)  = 0;
  virtual void set_sensitive(bool b)          = 0;
  virtual void set_readonly(bool b) {}
  virtual void update_langue() {}
  virtual void maj_langue() {update_langue();}
  virtual bool is_valid();
  bool on_focus_in(GdkEventFocus *gef);
protected:

  Attribute *model;
  std::string tp;
  virtual void on_event(const ChangeEvent &ce) = 0;
  bool lock;
  friend class AttributeListView;
private:
  /** Choose the best appropriate view type for the given attribute schema */
  static VueGenerique::WidgetType choose_view_type(refptr<AttributeSchema> as, bool editable = true);

};


class VueListeChamps: public VueGenerique,
                      private CListener<ChangeEvent>,
                      private CListener<FocusInEvent>,
                      public  CProvider<FocusInEvent>,
                      public  CProvider<KeyPosChangeEvent>,
                      private CListener<KeyPosChangeEvent>
{
public:
  VueListeChamps(Node &data_model, Node &view_model, Controleur *controler = nullptr);
  Gtk::Widget *get_gtk_widget();
  ~VueListeChamps();

  void set_sensitive(bool sensitive);

private:
  void on_event(const KeyPosChangeEvent &kpce){CProvider<KeyPosChangeEvent>::dispatch(kpce);}
  void on_event(const FocusInEvent &fie){CProvider<FocusInEvent>::dispatch(fie);}
  void on_event(const ChangeEvent &ce);
  void update_langue();

  Gtk::Table table;
  struct ChampsCtx
  {
    Gtk::Label label, label_unit;
    Gtk::Alignment align[3];
    AttributeView *av;
  };
  std::vector<ChampsCtx *> champs;
};

class AttributeListView: public Gtk::VBox,
                         private CListener<ChangeEvent>,
                         private CListener<FocusInEvent>,
                         public  CProvider<FocusInEvent>,
                         public  CProvider<KeyPosChangeEvent>,
                         private CListener<KeyPosChangeEvent>
{
public:
  AttributeListView();
  void init(Node model,
            const NodeViewConfiguration &config);
  AttributeListView(Node model,
                    const NodeViewConfiguration &config);

  ~AttributeListView();

  void update_langue();
  bool is_valid();

  void set_sensitive(bool sensitive);

  bool has_atts, has_indics, has_commands;

private:
  void on_event(const KeyPosChangeEvent &kpce){CProvider<KeyPosChangeEvent>::dispatch(kpce);}
  void on_event(const FocusInEvent &fie){CProvider<FocusInEvent>::dispatch(fie);}

  void on_the_realisation();
  void on_event(const ChangeEvent &ce);
  void on_b_command(std::string name);


  Gtk::ScrolledWindow scroll;
  Gtk::VBox           the_vbox;

  NodeViewConfiguration config;
  Node model;
  bool show_desc;
  bool small_strings;
  bool show_separator;
  int nb_columns;
  Gtk::Table table1, table2;

  Gtk::VSeparator separator;


  struct ViewElem
  {
    AttributeView *av;
    Gtk::Label *desc;
  };

  std::vector<ViewElem> attributes_view;

  bool list_is_sensitive;

  Gtk::Table table_indicators;
  Gtk::HButtonBox  box_actions;

  Gtk::HBox hbox;
  JFrame frame_att, frame_indicators, frame_actions;

  Gtk::Label label_desc;

  std::vector<Gtk::Button *> buttons;

  std::string class_name() const {return "AttributeListView";}
};




class SelectionView
  : public JFrame, 
    private CListener<ChangeEvent>
{
public:
  SelectionView();
  SelectionView(Node model);
  void setup(Node model);
private:
  
  void init();
  void update_view();
  void clear_table();
  void on_selection_changed();
  NodeSchema *get_selected();
  std::string class_name() const {return "SelectionView";}
  void on_event(const ChangeEvent &pce){update_view();}
  void set_selection(NodeSchema *&option);
  void on_cell_toggled(const Glib::ustring& path);

  class ModelColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:
      ModelColumns(){add(m_col_use); add(m_col_name); add(m_col_ptr); add(m_col_desc);}
      Gtk::TreeModelColumn<bool> m_col_use;
      Gtk::TreeModelColumn<Glib::ustring> m_col_name;
    Gtk::TreeModelColumn<NodeSchema *> m_col_ptr;
      Gtk::TreeModelColumn<Glib::ustring> m_col_desc;
  };

  Node model;
  bool lock;
  Gtk::ScrolledWindow scroll;
  Gtk::TreeView tree_view;
  Glib::RefPtr<Gtk::TreeStore> tree_model;
  ModelColumns columns;
};



class EVSelectionChangeEvent
{
public:
  Node selection;
};

class NodeView: private CListener<ChangeEvent>,
                public  CProvider<EVSelectionChangeEvent>,
                private CListener<KeyPosChangeEvent>,
                public  CProvider<KeyPosChangeEvent>,
                public VueGenerique
{
public:
  NodeView();
  NodeView(Node model);
  NodeView(Gtk::Window *mainWin, Node model);
  NodeView(Gtk::Window *mainWin, Node model,
           const NodeViewConfiguration &config);

  int init(Node modele, const NodeViewConfiguration &config = NodeViewConfiguration(), Gtk::Window *mainWin = nullptr);

  ~NodeView();
  Gtk::Widget *get_gtk_widget(){return get_widget();}
  Gtk::Widget *get_widget();
  void update_langue();
  void set_sensitive(bool sensitive);
  bool is_valid();
  void set_selection(Node reg);

  void maj_langue();

  static std::string mk_label(const Localized &l);
  static std::string mk_label_colon(const Localized &l);

protected:
  
private:

  int init(Gtk::Window *mainWin,
           Node model,
           const NodeViewConfiguration &config);

  NodeViewConfiguration config;
  int nb_columns;
  bool small_strings;
  int affichage;
  bool only_attributes;
  bool show_children;
  bool has_attributes;
  Gtk::Window *mainWin;
  bool show_separator;
  bool has_optionnals;
  class ModelColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:
      
    ModelColumns()
    { add(m_col_name); add(m_col_ptr); add(m_col_pic);}
    Gtk::TreeModelColumn<Glib::ustring> m_col_name;
    Gtk::TreeModelColumn<Node> m_col_ptr;
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> > m_col_pic;
  };
  std::deque<std::pair<Glib::RefPtr<Gdk::Pixbuf>, NodeSchema *> > pics;
  std::deque<NodeSchema *> pics_done;

  Glib::RefPtr<Gdk::Pixbuf> get_pics(NodeSchema *&schema);
  bool has_pic(NodeSchema *&schema);
  void load_pics(NodeSchema *&sc);
  void on_event(const ChangeEvent &ce);
  std::string class_name() const {return "NodeView";}
  virtual void on_treeview_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
  class MyTreeView : public Gtk::TreeView
  {
  public:
        MyTreeView();
        void init(NodeView *parent);
        MyTreeView(NodeView *parent);
        virtual bool on_button_press_event(GdkEventButton *ev);
  private:
      NodeView *parent;
  };

  class MyTreeModel: public Gtk::TreeStore
  {
  public:
    MyTreeModel(NodeView *parent, ModelColumns *cols);
  protected:
    virtual bool row_draggable_vfunc(const Gtk::TreeModel::Path& path) const;
    virtual bool row_drop_possible_vfunc(const Gtk::TreeModel::Path& dest, const Gtk::SelectionData& selection_data) const;
    virtual bool drag_data_received_vfunc( const TreeModel::Path&  	dest,
					   const Gtk::SelectionData&  	selection_data); 		
  private:
    NodeView *parent;
    ModelColumns *cols;
  };

  void populate();
  void populate(Node m, Gtk::TreeModel::Row row);
  virtual void on_selection_changed();
  Node get_selection();
  void remove_element(Node elt);
  void on_event(const KeyPosChangeEvent &kpce);
  void add_element(std::pair<Node, SubSchema *>);
  int  set_selection(Gtk::TreeModel::Row &root, Node reg, std::string path);

  void setup_row_view(Node ptr);
  void populate_notebook();
  void on_down(Node child);
  void on_up(Node child);
  bool lock;
  NodeSchema *schema;
  Node    model;
  AttributeListView table;
  Gtk::HBox  hbox;
  Gtk::Notebook notebook;
  JFrame      frame_attributs;
  bool show_desc;
  Gtk::Label label_description;
  Gtk::VBox vbox;
  Gtk::HPaned hpane;
  Gtk::ScrolledWindow scroll;
  ModelColumns columns;
  Glib::RefPtr<Gtk::TreeStore> tree_model;
  Gtk::Menu popup_menu;
  MyTreeView tree_view;
  JFrame tree_frame, properties_frame;
  NodeView *rp;
  SelectionView *option_view;
  std::vector<NodeView *> sub_views;
};

class NodeChangeEvent
{
public:
  Node source;
};

class NodeDialog: private CListener<ChangeEvent>,
                     public  CProvider<NodeChangeEvent>,
                     private CListener<KeyPosChangeEvent>,
                     public  DialogManager::Placable
{
public:
  void force_scroll(int dx, int dy);
  void unforce_scroll();
  /** @returns 0 -> ok, -1 : cancel */
  static int display_modal(Node model, bool fullscreen = false, Gtk::Window *parent_window = nullptr);
  static NodeDialog *display(Node model);
  ~NodeDialog();
  void maj_langue();
private:
  void on_event(const KeyPosChangeEvent &kpce);
  void on_event(const ChangeEvent &ce);
  NodeDialog(Node model, bool modal, Gtk::Window *parent_window = nullptr);
  void on_b_close();
  void on_b_apply();
  void on_b_valid();
  void update_view();
  bool on_focus_in(GdkEventFocus *gef);
  bool on_focus_out(GdkEventFocus *gef);
  Gtk::Window *get_window();
  bool on_expose_event2(const Cairo::RefPtr<Cairo::Context> &cr);
  void remove_widgets();
  void add_widgets();

  Gtk::Window *window;
  Gtk::Box   *vb;
  Node model;
  Node backup;
  bool lock, u2date;
  Gtk::VBox vbox;
  NodeView *ev;
  Gtk::HButtonBox hbox;
  Gtk::Button b_close;
  Gtk::Button b_apply, b_valid;
  Gtk::Toolbar toolbar;
  Gtk::ToolButton tool_valid, tool_cancel;
  Gtk::SeparatorToolItem sep2;
  Gtk::Label label_title;

  Gtk::Window wnd;
  Gtk::Dialog dlg;
  Gtk::ScrolledWindow scroll;
  bool modal;
  Gtk::Button *b_apply_ptr;
  Gtk::Button *b_close_ptr;
  //bool vkb_displayed;
  bool result_ok;
  //bool pseudo_dialog;

  VirtualKeyboard keyboard;
  Gtk::VSeparator keyboard_separator;
  bool fullscreen;
  bool exposed;
  int lastx, lasty;
  Gtk::Alignment kb_align;
};

struct AppliViewPrm
{
  AppliViewPrm();

  GColor background_color;


  /** @brief Color for the labels */
  bool overwrite_system_label_color;
  GColor att_label_color;

  /** If fixed size, screen is constrained to rect(ox,oy,dx,dy) */
  bool fixed_size;
  int dx, dy;
  int ox, oy;

  bool fullscreen;
  bool use_touchscreen;
  bool inverted_colors;
  bool use_decorations;

  bool use_button_toolbar;

  std::string img_cancel;
  std::string img_validate;

  /* minimum space from bottom */
  //uint32_t minimum_space_bottom;

  /* minimum space between keyboard and window */
  //uint32_t minimum_space_middle;

  /** Virtual keyboard below the apply / validate buttons */
  bool vkeyboard_below;
};

extern AppliViewPrm appli_view_prm;


}
}


#endif
