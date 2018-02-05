#include "mmi/stdview.hpp"
#include "mmi/ColorCellRenderer2.hpp"
#include "mmi/renderers.hpp"
#include "mmi/stdview-fields.hpp"
#include <gtkmm/cellrenderercombo.h>

#include "comm/serial.hpp"

#include <string.h>
//#include <malloc.h>
#include <stdlib.h>
#include <limits.h>

using namespace std;

namespace utils
{
namespace mmi
{

using namespace fields;



static string wtypes[((int) VueGenerique::WIDGET_MAX) + 1] =
{
  "vue-modele",
  "nullptr-widget", 
  "champs",
  "liste-champs",
  "indicateur",
  "bouton",
  "disposition-bordure",
  "disposition-grille",
  "disposition-fixe",
  "disposition-trig",
  "notebook",
  "panneau", // ?
  "image",
  "label",
  "combo",
  "decimal",
  "hexa",
  "float",
  "radio",
  "switch",
  "cadre",
  "choice",
  "bytes",
  "text",
  "string",
  "color",
  "date",
  "folder",
  "file",
  "serial",
  "list-layout",
  "disposition-verticale",
  "disposition-horizontale",
  "boite-boutons",
  "vue-speciale",
  "separateur-horizontal",
  "separateur-vertical",
  "table",
  "led",
  "invalide"
};








#define AFFICHAGE_REGLIST             0
#define AFFICHAGE_DESCRIPTION_REGLIST 1
#define AFFICHAGE_TREE                2
#define AFFICHAGE_OPTIONS             3

#define AFFICHAGE_MAX                 3
static const std::string aff_mode_names[4] = { "REGLIST", "DESC_REGLIST",
    "TREE", "OPTIONS" };

AppliViewPrm appli_view_prm;

AppliViewPrm::AppliViewPrm() {
  fixed_size = false;
  ox = 0;
  oy = 0;
  dx = 500;
  dy = 500;
  fullscreen = false;
  use_touchscreen = false;
  inverted_colors = false;
  use_decorations = true;
  vkeyboard_below = false;
  background_color.red = 255;
  background_color.green = 255;
  background_color.blue = 255;
  use_button_toolbar = false;
  overwrite_system_label_color = false;
}

static void update_description_label(Gtk::Label *res, Localized &loc)
{
  res->set_line_wrap(true);
  res->set_justify(Gtk::JUSTIFY_FILL);
  res->set_use_markup(true);
  //res->set_vexpand(false);
  res->set_size_request(250,-1);
  res->set_markup(loc.get_description(Localized::LANG_CURRENT));
}

static Gtk::Label *make_description_label(Localized &loc)
{
  Gtk::Label *label = new Gtk::Label();
  update_description_label(label, loc);
  return label;
}

// Pas d'enfants et moins de deux attributs
static bool is_tabular(NodeSchema *&es) 
{
  if (es->children.size() > 0)
    return false;
  if (es->attributes.size() > 4)
    return false;
  return true;
}

string NodeView::mk_label(const Localized &l)
{
  string traduction = l.get_localized();

  /*if (langue.has_item(traduction))
    traduction = langue.getItem(traduction);*/
  if ((traduction[0] >= 'a') && (traduction[0] <= 'z'))
    traduction[0] += 'A' - 'a';
  if ((uint8_t) traduction[0] == /*'Ã©'*/0x82)
    traduction[0] = 'E';

  return traduction;
}

std::string NodeView::mk_label_colon(const Localized &l)
{
  std::string traduction = mk_label(l);
  if(Localized::current_language == Localized::LANG_FR)
    return traduction + " : ";
  return traduction + ": ";
}


static std::string mk_html_tooltip(refptr<AttributeSchema> as)
{
  std::string desc = as->name.get_description(Localized::LANG_CURRENT);
  std::string title = NodeView::mk_label(as->name);

# ifdef LINUX
  /* Black background on linux */
  std::string color = "#60f0ff";
# else
  std::string color = "#006080";
# endif

  std::string tooltip = std::string("<b><span foreground='" + color + "'>") + title
    + std::string("</span></b>\n") + desc;

  if (as->enumerations.size() > 0) 
  {
    for (unsigned int k = 0; k < as->enumerations.size(); k++) 
    {
        Enumeration &e = as->enumerations[k];
        if (e.name.has_description()) 
          tooltip += std::string("\n<b>") + e.name.get_localized()
            + " : </b>" + e.name.get_description();
    }
  }
  return tooltip;
}

NodeViewConfiguration::NodeViewConfiguration() {
  show_desc = false;
  show_main_desc = false;
  show_children = false;
  small_strings = false;
  show_separator = true;
  disable_all_children = false;
  display_only_tree = false;
  expand_all = false;
  nb_attributes_by_column = 8;
  nb_columns = 1;
  horizontal_splitting = true;
  table_width = -1;
  table_height = -1;
}

std::string NodeViewConfiguration::to_string() const {
  char buf[300];
  sprintf(buf, "show children = %s, disable all child = %s, n cols = %d.",
      show_children ? "true" : "false", disable_all_children ? "true" : "false",
      nb_columns);
  return std::string(buf);
}



AttributeView::~AttributeView()
{
  model->CProvider<ChangeEvent>::remove_listener(this);
}

bool AttributeView::is_valid() 
{
  return true;
}

bool AttributeView::on_focus_in(GdkEventFocus *gef) 
{
  if(appli_view_prm.use_touchscreen) 
  {
    std::vector < std::string > vchars;
    model->schema->get_valid_chars(vchars);
    KeyPosChangeEvent kpce;
    kpce.vchars = vchars;
    CProvider < KeyPosChangeEvent > ::dispatch(kpce);
  }
  return true;
}

VueGenerique::WidgetType AttributeView::choose_view_type(refptr<AttributeSchema> as, bool editable)
{
  if((as->type == TYPE_BOOLEAN) && (!editable))
    return VueGenerique::WIDGET_LED;

  if(!editable)
    return VueGenerique::WIDGET_FIXED_STRING;

  if((as->constraints.size() > 0) && (as->type != TYPE_COLOR))
    return VueGenerique::WIDGET_COMBO;

  else if ((as->enumerations.size() > 0) && (as->has_max)
      && (as->max < 100))
    return VueGenerique::WIDGET_COMBO;

  switch (as->type)
  {
    case TYPE_INT:
      if (as->is_bytes)
        return VueGenerique::WIDGET_BYTES;
      else if (as->is_hexa)
        return VueGenerique::WIDGET_HEXA_ENTRY;
      else
        return VueGenerique::WIDGET_DECIMAL_ENTRY;

    case TYPE_STRING:
      if (as->formatted_text)
        return VueGenerique::WIDGET_TEXT;
      else
        return VueGenerique::WIDGET_STRING;

    case TYPE_BOOLEAN:
      return VueGenerique::WIDGET_SWITCH;

    case TYPE_FLOAT:
      return VueGenerique::WIDGET_FLOAT_ENTRY;

    case TYPE_COLOR:
      return VueGenerique::WIDGET_COLOR;

    case TYPE_DATE:
      return VueGenerique::WIDGET_DATE;

    case TYPE_FOLDER:
      return VueGenerique::WIDGET_FOLDER;

    case TYPE_FILE:
      return VueGenerique::WIDGET_FILE;

    case TYPE_SERIAL:
      return VueGenerique::WIDGET_SERIAL;

    default:
    {
      string s = as->to_string();
      avertissement("choose view type: unable to select one, schema:\n%s", s.c_str());
      return VueGenerique::WIDGET_NULL;
    }
  }
}

AttributeView *AttributeView::factory(Node model, Node view)
{
  XPath path;

  if(view.has_attribute("model"))
    path = view.get_attribute_as_string("model");
  else
    path = view.get_attribute_as_string("modele");

  Node owner = model.get_child(path.remove_last());
  refptr<AttributeSchema> as = owner.schema()->get_attribute(path.get_last());
  Attribute *att = owner.get_attribute(path.get_last());

  VueGenerique::WidgetType type = VueGenerique::WIDGET_AUTO;
  bool editable = view.get_attribute_as_boolean("editable");

  std::string type_str = view.get_attribute_as_string("type");

  if(type_str.size() > 0)
    type = VueGenerique::desc_vers_type(type_str);

  if(type == VueGenerique::WIDGET_AUTO)
    type = choose_view_type(as, editable);

  //if(!editable)
    //type = VueGenerique::WIDGET_INDICATOR;

  std::string s = VueGenerique::type_vers_desc(type);

  trace_verbeuse("factory(): att type: %d (%s).", (int) type, s.c_str());

  AttributeView *res = nullptr;

  switch(type)
  {
    case VueGenerique::WIDGET_LED:
    {
      res = new VueLed(att, editable);
      break;
    }
    case VueGenerique::WIDGET_FIXED_STRING:
    {
      res = new VueChaineConstante(att);
      break;
    }
    case VueGenerique::WIDGET_STRING:
    {
      res = new VueChaine(att);
      break;
    }
    case VueGenerique::WIDGET_TEXT:
    {
      erreur("A FAIRE : vue texte.");
      //res = new VueTexte(att);
      break;
    }
    case VueGenerique::WIDGET_DECIMAL_ENTRY:
    {
      res = new VueDecimal(att);
      break;
    }
    case VueGenerique::WIDGET_FLOAT_ENTRY:
    {
      res = new VueFloat(att, view);
      break;
    }
    case VueGenerique::WIDGET_HEXA_ENTRY:
    {
      res = new VueHexa(att);
      break;
    }
    case VueGenerique::WIDGET_SWITCH:
    {
      res = new utils::mmi::fields::VueBouleen(att, false);
      break;
    }
    case VueGenerique::WIDGET_COMBO:
    {
      res = new VueCombo(att);
      break;
    }
    case VueGenerique::WIDGET_FILE:
    {
      res = new VueFichier(att, false);
      break;
    }
    default:
      avertissement("factory(): unamanaged att type: %d (%s).", (int) type, s.c_str());
  }

  if(res != nullptr)
    att->CProvider<ChangeEvent>::add_listener(res);

  return res;
}


AttributeView *AttributeView::build_attribute_view(Attribute *model,
                                                   const NodeViewConfiguration &config,
                                                   Node parent)
{
  AttributeView *res = nullptr;
  AttributeSchema &schema = *(model->schema);

  bool has_sub_schema = false;
  for(unsigned int i = 0; i < schema.enumerations.size(); i++)
  {
    if(schema.enumerations[i].schema != nullptr)
    {
      has_sub_schema = true;
      break;
    }
  }

  if(has_sub_schema)
  {
    res = new VueChoix(model, parent, config);
  }
  else if((schema.constraints.size() > 0) && (schema.type != TYPE_COLOR))
  {
    res = new VueCombo(model);
  }
  else if ((schema.enumerations.size() > 0) && (schema.has_max)
      && (schema.max < 100))
  {
    res = new VueCombo(model);
  }
  else
  {
    switch (schema.type)
    {
    case TYPE_INT:
    {
      if(schema.is_instrument)
        res = new VueChaineConstante(model);
      else if (schema.is_bytes)
        res = new VueOctets(model);
      else if (schema.is_hexa)
        res = new VueHexa(model);
      else
        res = new VueDecimal(model, 1);
      break;
    }
    case TYPE_STRING:
    {
      if(schema.is_instrument)
        res = new VueChaineConstante(model);
      else if(schema.formatted_text)
        res = new VueTexte(model, config.small_strings);
      else
        res = new VueChaine(model, config.small_strings);
      break;
    }
    case TYPE_BOOLEAN:
    {
      if (schema.is_instrument)
        res = new VueLed(model);
      else
        res = new VueBouleen(model);
      break;
    }
    case TYPE_FLOAT:
    {
      if(schema.is_instrument)
        res = new VueChaineConstante(model);
      else
        res = new VueFloat(model);
      break;
    }
    case TYPE_COLOR:
    {
      res = new VueChoixCouleur(model);
      break;
    }
    case TYPE_DATE:
    {
      if(schema.is_instrument)
        res = new VueChaineConstante(model);
      else
        res = new VueDate(model);
      break;
    }
    case TYPE_FOLDER:
    {
      if(schema.is_instrument)
        res = new VueChaineConstante(model);
      else
        res = new VueDossier(model);
      break;
    }
    case TYPE_FILE:
    {
      if(schema.is_instrument)
        res = new VueChaineConstante(model);
      else
        res = new VueFichier(model);
      break;
    }
    case TYPE_SERIAL:
    {
      if(schema.is_instrument)
        res = new VueChaineConstante(model);
      else
        res = new VueSelPortCOM(model);
      break;
    }
    default:
      break;
    }
  }

  model->CProvider < ChangeEvent > ::add_listener(res);
  return res;
}





void NodeView::load_pics(NodeSchema *&sc)
{
  for (uint32_t i = 0; i < pics_done.size(); i++) {
    if (pics_done[i] == sc)
      return;
  }

  pics_done.push_back(sc);

  if ((!has_pic(sc)) && sc->has_icon()) {
    std::pair<Glib::RefPtr<Gdk::Pixbuf>, NodeSchema *> p;
    p.second = sc;
    std::string filename = utils::get_img_path() + files::get_path_separator()
        + sc->icon_path;
    //infos(std::string("Loading pic: ") + filename);
    if (!files::file_exists(filename))
      avertissement("picture loading: " + filename + " not found.");
    else {
      p.first = Gdk::Pixbuf::create_from_file(filename.c_str());
      pics.push_back(p);
    }
  }

  for (unsigned int i = 0; i < sc->children.size(); i++)
    load_pics(sc->children[i].ptr);

  /*if((!has_pic(sc)) && sc->has_icon())
   {
   std::pair<Glib::RefPtr<Gdk::Pixbuf>, NodeSchema *> p;
   p.second = sc;
   std::string filename = IMG_DIR + "/img/" + sc->icon_path;
   infos(std::string("Loading pic: ") + filename);
   if(!Util::file_exists(filename))
   erreur("picture loading: " + filename + " not found.");
   else
   {
   p.first  = Gdk::Pixbuf::create_from_file(filename);
   pics.push_back(p);
   }
   for(unsigned int i = 0; i < sc->children.size(); i++)
   load_pics(sc->children[i].ptr);
   }*/
}

void NodeView::maj_langue()
{
  table.update_langue();
  for (unsigned int i = 0; i < sub_views.size(); i++)
    sub_views[i]->update_langue();
}

void NodeView::update_langue() {
  maj_langue();
}

void NodeView::set_sensitive(bool sensitive)
{
  table.set_sensitive(sensitive);
  for(unsigned int i = 0; i < sub_views.size(); i++)
    sub_views[i]->set_sensitive(sensitive);
}

static bool is_notebook_display(const SubSchema &ss)
{
  if(ss.display_tree)
    return false;
  if ((ss.min == ss.max) && (ss.min >= 1) && (ss.min <= 2))
    return true;
  if ((ss.min == 0) && (ss.max == 1))
    return true;
  return false;
}

static bool can_tab_display(const SubSchema &ss)
{
  if (ss.display_tree)
    return false;
  if (ss.ptr->children.size() > 0)
    return false;
  if(ss.ptr->attributes.size() > 4)
    return false;
  return true;
}

void NodeView::on_event(const KeyPosChangeEvent &kpce)
{
  utils::CProvider<KeyPosChangeEvent>::dispatch(kpce);
}

NodeView::MyTreeModel::MyTreeModel(NodeView *parent, ModelColumns *cols)
{
  this->parent = parent;
  set_column_types(*cols);
}

bool NodeView::MyTreeModel::row_draggable_vfunc(
    const Gtk::TreeModel::Path& path) const {
  return true;
}

bool NodeView::MyTreeModel::row_drop_possible_vfunc(
    const Gtk::TreeModel::Path& dest,
    const Gtk::SelectionData& selection_data) const {
  // TODO: check if droppable
  Gtk::TreeModel::Path dest_parent = dest;
  NodeView::MyTreeModel* unconstThis = const_cast<NodeView::MyTreeModel*>(this);
  const_iterator iter_dest_parent = unconstThis->get_iter(dest_parent);
  if (iter_dest_parent) {
    Row row = *iter_dest_parent;
    Node target = row[parent->columns.m_col_ptr];
    //target = target.parent();

    //infos("drop to: ");
    //utils::infos("me.");

    Glib::RefPtr<Gtk::TreeModel> refThis = Glib::RefPtr<Gtk::TreeModel> (const_cast<NodeView::MyTreeModel*>(this));
    refThis->reference(); //, true /* take_copy */)
    Gtk::TreeModel::Path path_dragged_row;
    Gtk::TreeModel::Path::get_from_selection_data(selection_data, refThis,
        path_dragged_row);
    const_iterator iter = refThis->get_iter(path_dragged_row);
    Row row2 = *iter;
    Node src = row2[parent->columns.m_col_ptr];
    //infos("drop from: ");
    //src.infos("me.");

    std::string src_type = src.schema()->name.get_id();
    if (target.schema()->has_child(src_type)) {
      infos("%s -> %s: ok.", src_type.c_str(),
          target.schema()->name.get_id().c_str());
      return true;
    }
    infos("%s -> %s: nok.", src_type.c_str(),
        target.schema()->name.get_id().c_str());
    return false;
  }
  return false;
}

bool NodeView::MyTreeModel::drag_data_received_vfunc(
    const TreeModel::Path& dest, const Gtk::SelectionData& selection_data) {
  infos("drag data received.");
  Gtk::TreeModel::Path dest_parent = dest;
  NodeView::MyTreeModel* unconstThis = const_cast<NodeView::MyTreeModel*>(this);
  const_iterator iter_dest_parent = unconstThis->get_iter(dest_parent);
  if (iter_dest_parent) {
    Row row = *iter_dest_parent;
    Node target = row[parent->columns.m_col_ptr];

    //infos("drop to: ");
    //target.infos("me.");

    Glib::RefPtr < Gtk::TreeModel > refThis = Glib::RefPtr < Gtk::TreeModel
        > (const_cast<NodeView::MyTreeModel*>(this));
    refThis->reference(); //, true /* take_copy */)
    Gtk::TreeModel::Path path_dragged_row;
    Gtk::TreeModel::Path::get_from_selection_data(selection_data, refThis,
        path_dragged_row);

    const_iterator iter = refThis->get_iter(path_dragged_row);
    Row row2 = *iter;
    Node src = row2[parent->columns.m_col_ptr];

    //infos("drop from: ");
    //src.infos("me.");

    infos("dragging..");
    src.parent().remove_child(src);
    Node nv = target.add_child(src);
    infos("done.");
    parent->populate(); //update_view();
    parent->set_selection(nv);
    return true;
  }
  return false;
}

NodeView::NodeView(Gtk::Window *mainWin, Node model)//: tree_view(this)
:  table(model, NodeViewConfiguration())//, tree_view(this)
{
  NodeViewConfiguration config;
  init(mainWin, model, config);
}



NodeView::NodeView()
{
}

NodeView::NodeView(Node model) :
    table(model, NodeViewConfiguration())
{
  NodeViewConfiguration config;
  init(mainWindow, model, config);
}

NodeView::NodeView(Gtk::Window *mainWin, Node model,
    const NodeViewConfiguration &config) :
    table(model, config)
{
  init(mainWin, model, config);
}

// public
int NodeView::init(Node modele,
                   const NodeViewConfiguration &config,
                   Gtk::Window *mainWin)
{
  table.init(modele, config);
  return init(mainWin, modele, config);
}

// private
int NodeView::init(Gtk::Window *mainWin,
                   Node model,
                   const NodeViewConfiguration &config)
{
  tree_view.init(this);
  this->config = config;
  // (1) Inits diverses
  //infos("Creation...");
  this->nb_columns = config.nb_columns;
  this->show_children = config.show_children;
  this->mainWin = mainWin;
  this->model = model;
  this->schema = model.schema();
  this->show_desc = config.show_desc;
  this->small_strings = false;
  lock = false;
  only_attributes = true;
  this->show_separator = config.show_separator;
  has_optionnals = false;

  bool tree_display = false;

  if ((schema->children.size() > 0)
      && !config.disable_all_children)
  {
    for (unsigned int i = 0; i < schema->children.size(); i++)
    {
      SubSchema &ss = schema->children[i];
      if ((!is_notebook_display(ss)) && (!ss.is_hidden)
          && !config.disable_all_children && !can_tab_display(ss))
      {
        tree_display = true;
        /*infos("Tree display because of %s, min = %d, max = %d.",
            ss.name.get_id().c_str(), ss.min, ss.max);*/
      }
      if ((ss.min == 0) && (ss.max == 1))
        has_optionnals = true;
    }
  }

  // (2) Determine le type d'affichage
  // Avec des enfant (le + prioritaire), HPANE(arbre,rpanel)
  if (tree_display)
  {
    affichage = AFFICHAGE_TREE;
    //infos("Aff tree");
  }
  else
  {
    affichage = AFFICHAGE_TREE;
    if (!config.disable_all_children)
    {
      for (unsigned int i = 0; i < schema->children.size(); i++)
      {
        SubSchema ss = schema->children[i];
        if (!ss.is_exclusive)
          affichage = AFFICHAGE_OPTIONS;
      }
    } else
    {
    }

    // Mode options
    //if((schema->optionals.size() > 0) || ((schema->children.size() > 0) && show_children))
    {
      //affichage = AFFICHAGE_OPTIONS;
      //model->infos("Aff options");
    }
    if (affichage == AFFICHAGE_TREE)
    {
      // Attributs + description dans leur frame
      if (schema->has_description())
      {
        affichage = AFFICHAGE_REGLIST;
        //model.infos("Aff reg desc list");
      }
      // Juste attributs, sans frame (table)
      else
      {
        affichage = AFFICHAGE_REGLIST;
        //model.infos("Aff reg list");
      }
    }
  }

  //////////////////////////////////////////////////////////////
  ///////        AFFICHAGE ARBRE UNIQUEMENT              ///////
  //////////////////////////////////////////////////////////////
  if (affichage == AFFICHAGE_TREE)
  {
    //infos("Creation: arbre...");
    pics_done.clear();
    load_pics(schema);
    //infos("Creation: load pics ok.");
    rp = nullptr;

    if (!config.display_only_tree)
    {
      //infos("display_only_tree = false.");
      //hpane.add1(tree_frame);
      //hpane.add2(properties_frame);

      //hpane.pack1(tree_frame, Gtk::FILL);
      //hpane.pack2(properties_frame, Gtk::EXPAND);

      //hpane.pack1(tree_frame, true, false);

      hpane.pack1(tree_frame, false, /*false*/true);
      hpane.pack2(properties_frame, true, /*false*/true);
      tree_frame.set_size_request(300, 300); //-1);
      hpane.set_border_width(5);
      hpane.set_position(300);
    }
    //else
    //infos("display_only_tree = true.");
    tree_view.set_headers_visible(false);
    tree_view.set_enable_tree_lines(true);

    scroll.add(tree_view);
    scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scroll.set_border_width(5);
    tree_frame.add(scroll);
    //tree_model = Gtk::TreeStore::create(columns);
    tree_model = Glib::RefPtr < MyTreeModel > (new MyTreeModel(this, &columns));

    tree_view.set_model(tree_model);
    populate();

    Gtk::TreeViewColumn *tvc = Gtk::manage(new Gtk::TreeViewColumn());
    Gtk::CellRendererPixbuf *crp = Gtk::manage(new Gtk::CellRendererPixbuf());
    tvc->pack_start(*crp, false);
    tvc->add_attribute(crp->property_pixbuf(), columns.m_col_pic);

    Gtk::CellRendererText *crt = new Gtk::CellRendererText();
    tvc->pack_start(*crt, true);
    tvc->add_attribute(crt->property_markup(), columns.m_col_name);
    tree_view.append_column(*tvc);

#   if 0
    tree_view.append_column("", columns.m_col_pic);
    {
      Gtk::CellRendererText *render = Gtk::manage(new Gtk::CellRendererText());
      Gtk::TreeView::Column *viewcol = Gtk::manage(new Gtk::TreeView::Column ("", *render));
      viewcol->add_attribute(render->property_markup(), columns.m_col_name);
      tree_view.append_column(*viewcol);
    }
#   endif
    tree_view.signal_row_activated().connect(
        sigc::mem_fun(*this, &NodeView::on_treeview_row_activated));

    Glib::RefPtr < Gtk::TreeSelection > refTreeSelection =
        tree_view.get_selection();
    refTreeSelection->signal_changed().connect(
        sigc::mem_fun(*this, &NodeView::on_selection_changed));

    popup_menu.accelerate(tree_view);
    //infos("Creation: arbre ok.");
  }

  //////////////////////////////////////////////////////////////
  ///////        AFFICHAGE OPTIONS UNIQUEMENT            ///////
  //////////////////////////////////////////////////////////////
  if (affichage == AFFICHAGE_OPTIONS) {
    only_attributes = false;
  }

  tree_view.enable_model_drag_source();
  tree_view.enable_model_drag_dest();

  //////////////////////////////////////////////////////////////
  ///////        CODE COMMUN                             ///////
  //////////////////////////////////////////////////////////////

# if 0
  frame_attributs.set_label("Attributs");
  frame_attributs.set_border_width(7);
  has_attributes = (model.get_attribute_count() > 0);

  if(schema->has_description())
  {
    if((affichage == AFFICHAGE_OPTIONS) && (!has_attributes))
    ;
    else
    {
      label_description.set_use_markup(true);
      label_description.set_markup(schema->description);
      vbox.pack_start(label_description, Gtk::PACK_SHRINK);
      only_attributes = false;
    }
  }
  // Si il y a des attributs, on ajoute la frame
  if(has_attributes)
  vbox.pack_start(frame_attributs, Gtk::PACK_SHRINK);
  // Si seulement des attributs, on retourne la table, pas la frame !
  if(!only_attributes)
  frame_attributs.add(table);
# endif

  has_attributes = (model.schema()->attributes.size() > 0);
  if (schema->has_description()) {
    if ((affichage == AFFICHAGE_OPTIONS) && (!has_attributes))
      ;
    else {
      only_attributes = false;
    }
  }

  /*if(model.schema()->name.has_description())
  {
    auto l = make_description_label(model.schema()->name);
    table.pack_end(*l, Gtk::PACK_SHRINK);
    //infos("Ajout description node.");
  }*/

  //////////////////////////////////////////////////////////////
  ///////        AFFICHAGE OPTIONS UNIQUEMENT            ///////
  //////////////////////////////////////////////////////////////
  if (affichage == AFFICHAGE_OPTIONS)
  {
    if (has_optionnals)
    {
      vbox.pack_start(table, Gtk::PACK_SHRINK);
      infos("Create option view...");
      option_view = new SelectionView(model);
      vbox.pack_start(*option_view, Gtk::PACK_EXPAND_WIDGET);
      

      bool display_notebook = false;
      // check if something to configure...
      for(unsigned int i = 0; i < schema->children.size(); i++)
      {
        SubSchema &ss = schema->children[i];
        if((ss.min == 0) && (ss.max == 1) && !(ss.ptr->is_empty()))
        {
          display_notebook = true;
          break;
        }
      }

      if(display_notebook)
      {
        hbox.pack_start(vbox, Gtk::PACK_SHRINK);
        hbox.pack_start(notebook, Gtk::PACK_EXPAND_WIDGET);
      }
      else
      {
        hbox.pack_start(vbox, Gtk::PACK_EXPAND_WIDGET);
      }
      notebook.set_scrollable(true);
    }
    else
    {
      if (config.horizontal_splitting)
      {
        hbox.pack_start(table, Gtk::PACK_SHRINK);
        hbox.pack_start(notebook, Gtk::PACK_SHRINK);
      }
      else
      {
        hbox.pack_start(vbox, Gtk::PACK_SHRINK);
        vbox.pack_start(table, Gtk::PACK_SHRINK);
        vbox.pack_start(notebook, Gtk::PACK_SHRINK);
      }
    }
    populate_notebook();
  }

  table.CProvider < KeyPosChangeEvent > ::add_listener(this);

  model.add_listener((CListener<ChangeEvent> *) this);

  /*infos("EVIEW for %s:\nconfig = %s,\nres = %s, has optionnals = %s",
   model.get_identifier().c_str(),
   config.to_string().c_str(),
   aff_mode_names[affichage].c_str(),
   has_optionnals ? "true" : "false");*/
  return 0;
}

bool NodeView::is_valid() {
  //infos("is_valid()...");
  for (unsigned int i = 0; i < sub_views.size(); i++) {
    if (!sub_views[i]->is_valid()) {
      avertissement("sub_view[%d] not valid.", i);
      return false;
    }
  }
  bool tvalid = table.is_valid();
  if (!tvalid) {
    avertissement("attribute list is not valid.");
  }
  return tvalid;
}

NodeView::~NodeView() {
  //infos("~NodeView..");
  model./*CProvider<ChangeEvent>::*/remove_listener(
      (CListener<ChangeEvent> *) this);
  //model./*CProvider<StructChangeEvent>::*/remove_listener((CListener<StructChangeEvent> *)this);
  for (unsigned int i = 0; i < sub_views.size(); i++)
    delete sub_views[i];
  sub_views.clear();
  //infos("~NodeView done.");
}

void NodeView::populate_notebook() {
  if (affichage != AFFICHAGE_OPTIONS)
    return;

  //infos("Populate notebook...");

  /* Remove old pages */
  int n = notebook.get_n_pages();
  for (int i = 0; i < n; i++)
    notebook.remove_page(0);

  /* Delete node views */
  for (unsigned int i = 0; i < sub_views.size(); i++) {
    delete sub_views[i];
  }
  sub_views.clear();

  /* Place sub-panels */
  for (unsigned int i = 0; i < schema->children.size(); i++)
  {
    SubSchema os = schema->children[i];

    //if (!(is_notebook_display(os) && model.has_child(os.name.get_id())
    //    && (!os.is_exclusive))) 
    //{
      //infos("not for notebook: %s.", os.name.get_id().c_str());
      //infos("OS = %s.", os.to_string().c_str());
      //if (!is_notebook_display(os))
      //  infos("!is_notebook_display");
      //if (!model.has_child(os.name.get_id()))
      // infos("no such child");
    //}

    if (is_notebook_display(os) && model.has_child(os.name.get_id())
        && (!os.is_exclusive)) 
    {

      if(os.ptr->is_empty())
        continue;

      uint32_t n = model.get_children_count(os.name.get_id());

      //infos("notebook of for %s: %d children.", os.name.get_id().c_str(), n);

      for (unsigned int j = 0; j < n; j++) {
        Node child = model.get_child_at(os.name.get_id(), j);

        NodeViewConfiguration vconfig = config;

        if (has_attributes)
          vconfig.horizontal_splitting = !vconfig.horizontal_splitting;

        NodeView *mv = new NodeView(mainWin, child, vconfig);
        NodeSchema *schem = schema->children[i].ptr;
        std::string tname;

        mv->CProvider < KeyPosChangeEvent > ::add_listener(this);

        tname = NodeView::mk_label(schem->name);

        if (n > 1)
          tname += std::string(" ") + utils::str::int2str(j + 1);
        sub_views.push_back(mv);

        Gtk::Alignment *align;
        align = new Gtk::Alignment(Gtk::ALIGN_START, Gtk::ALIGN_START, 1.0, 1.0);
        //0.0, 0.0);
        align->add(*(mv->get_widget()));

        Gtk::HBox *ybox;
        ybox = new Gtk::HBox();

        if (child.schema()->has_description()) {
          ybox->set_has_tooltip();
          ybox->set_tooltip_markup(child.schema()->name.get_description());
        }

        if (!schem->has_icon()) {
          ybox->pack_start(*(new Gtk::Label(" " + tname)));
          notebook.append_page(*align, *ybox);
          ybox->show_all_children();
        } else {
          std::string filename = utils::get_img_path()
              + files::get_path_separator() + schem->icon_path;
          //infos("img filename: %s.", filename.c_str());
          ybox->pack_start(*(new Gtk::Image(filename)));
          ybox->pack_start(*(new Gtk::Label(" " + tname)));
          notebook.append_page(*align, *ybox);
          ybox->show_all_children();
        }
      }
    }
  }
  hbox.show_all_children();
}

Gtk::Widget *NodeView::get_widget() {
  // Avec des enfant, HPANE(arbre,rpanel)
  if (affichage == AFFICHAGE_TREE) {
    if (config.display_only_tree)
      return &tree_frame;
    else
      return &hpane;
  }
  // Juste attributs, meme pas de description
  else if (affichage == AFFICHAGE_REGLIST)
    return &table;
  // Description + attributs dans leur frame
  else if (affichage == AFFICHAGE_DESCRIPTION_REGLIST)
    return &table;
  // Attributs + options a cocher + onglets
  else if (affichage == AFFICHAGE_OPTIONS)
    return &hbox;
  erreur("Type d'affichage inconnu.");
  return nullptr;
}

void NodeView::populate() {
  tree_model->clear();
  Gtk::TreeModel::Row row = *(tree_model->append());

  row[columns.m_col_name] = model.get_identifier(true, true);
  row[columns.m_col_ptr] = model;
  if (has_pic(schema))
    row[columns.m_col_pic] = get_pics(schema);
  populate(model, row);
}

static bool has_tree_child(Node m) {
  unsigned int i;

  for (i = 0; i < m.schema()->children.size(); i++) {
    SubSchema ss = m.schema()->children[i];
    if (ss.display_tab
        || (is_tabular(ss.ptr) && ((ss.min != 1) || (ss.max != 1))
            && (!ss.display_tree)))
      continue;
    if (m.has_child(ss.name.get_id()))
      return true;
  }
  return false;
}

void NodeView::populate(Node m, Gtk::TreeModel::Row row) {
  /* Add all the child of "m" node below the "row" */
  for (unsigned int i = 0; i < m.schema()->children.size(); i++) {
    SubSchema ss = m.schema()->children[i];
    NodeSchema *&schema = ss.ptr;

    //if((schema->attributes.size() == 1)
    //  && (schema->children.size() == 0))
    //continue;

    if (ss.display_tab
        || (is_tabular(schema) && ((ss.min != 1) || (ss.max != 1))
            && (!ss.display_tree)))
      continue;

    //if(ss.is_command)
    //continue;

    if (ss.is_exclusive)
      continue;

    //infos("populate: child %s..", ss.name.get_id().c_str());

    unsigned int n = m.get_children_count(ss.name.get_id());
    for (unsigned int j = 0; j < n; j++) {
      Node child = m.get_child_at(ss.name.get_id(), j);
      Gtk::TreeModel::Row subrow = *(tree_model->append(row.children()));

      std::string s = child.get_identifier(true, false);

      // if schema name equals sub node schema name ?????
      if (ss.name.get_id().compare(child.schema()->name.get_id())) {
        infos("apply localized.");
        s = ss.name.get_localized();
      }

      /* If has at least one child, display in bold font. */
      if (has_tree_child(child))
        s = "<b>" + s + "</b>";

      /*if(lst[j].get_children_count() > 0)
       {
       for(unsigned int k = 0; k < lst[j].get_children_count(); k++)
       {
       SubSchema &ss
       }*/

      /*SubSchema &ss = *(m.schema()->get_child(lst[j].schema()->name.get_id()));
       if(ss.display_tab
       || (is_tabular(schema)
       && ((ss.min != 1) || (ss.max != 1))
       && (!ss.display_tree)))
       ;
       else
       s = "<b>" + s + "</b>";*/
      //}
      subrow[columns.m_col_name] = s;
      subrow[columns.m_col_ptr] = child;

      if (has_pic(schema)) {
        //infos("Set pic for %s.", lst[j].get_identifier().c_str());
        subrow[columns.m_col_pic] = get_pics(schema);
      }
      populate(child, subrow);

      if (ss.display_unfold)
        //tree_view.expand_to_path(tree_model->get_path(subrow));
        tree_view.expand_row(tree_model->get_path(subrow), true);
      /*else
       m.erreur("NO UNFOLD.");*/
    }
  }

  if (config.expand_all)
    tree_view.expand_all();
}

void NodeView::on_event(const ChangeEvent &e)
{
  if ((e.type == ChangeEvent::CHILD_ADDED)
      || (e.type == ChangeEvent::CHILD_REMOVED))
  {
    string s = e.to_string();
    infos("got add/rem event: %s", s.c_str());
    if (e.path.length() == 2)
    {
      infos("on_event(StructChangeEvent)");
      if (affichage == AFFICHAGE_TREE)
        populate();
      else if (affichage == AFFICHAGE_OPTIONS)
        populate_notebook();
    }
  }

}

#if 0
void NodeView::on_event(const StructChangeEvent &e)
{
  /*if(*(e.get_owner()) == this->model)
   {
   infos("on_event(StructChangeEvent)");
   if(affichage == AFFICHAGE_TREE)
   populate();
   else if(affichage == AFFICHAGE_OPTIONS)
   populate_notebook();
   }*/
}
#endif

Node NodeView::get_selection() {
  Glib::RefPtr < Gtk::TreeSelection > refTreeSelection =
      tree_view.get_selection();
  Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
  Node result;
  if (iter) {
    Gtk::TreeModel::Row ligne = *iter;
    result = ligne[columns.m_col_ptr];
  }
  return result;
}

void NodeView::on_selection_changed() {
  if (!lock) {
    lock = true;
    Node m = get_selection();
    if (!m.is_nullptr())
      setup_row_view(m);
    lock = false;
  }
}

void NodeView::on_down(Node child) {
  infos("on_down..");
  Node nv = child.parent().down(child);
  populate();
  set_selection(nv);
}

void NodeView::on_up(Node child) {
  infos("on_up..");
  Node nv = child.parent().up(child);
  populate();
  set_selection(nv);
}

bool NodeView::MyTreeView::on_button_press_event(GdkEventButton *event) {
  //bool return_value = TreeView::on_button_press_event(event);

  // TODO: gtk3 port
# if 0

  if ((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
    Node m = parent->get_selection();
    if (m.is_nullptr()) {
      return TreeView::on_button_press_event(event);
      //return return_value;
    }

    Gtk::MenuList &menulist = parent->popup_menu.items();
    menulist.clear();

    Gtk::MenuElem mr(
        langue.get_item("Remove"),
        sigc::bind < Node
            > (sigc::mem_fun(parent, &NodeView::remove_element), m));
    if (parent->model != m) {
      menulist.push_back(mr);
      menulist.push_back(Gtk::Menu_Helpers::SeparatorElem());
    }

    if (!m.parent().is_nullptr()) {
      std::string sname = m.schema()->name.get_id();
      int n = m.parent().get_children_count(sname);
      if (n > 1) {
        int index = -1;
        for (int i = 0; i < n; i++) {
          if (m.parent().get_child_at(sname, i) == m) {
            index = i;
            break;
          }
        }

        if ((index != -1) && (index > 0))
          menulist.push_back(
              Gtk::MenuElem(
                  langue.get_item("up"),
                  sigc::bind < Node
                      > (sigc::mem_fun(parent, &NodeView::on_up), m)));

        if ((index != -1) && (index + 1 < n))
          menulist.push_back(
              Gtk::MenuElem(
                  langue.get_item("down"),
                  sigc::bind < Node
                      > (sigc::mem_fun(parent, &NodeView::on_down), m)));
      }
    }

    for (unsigned int i = 0; i < m.schema()->children.size(); i++) {
      SubSchema &sub_schema = m.schema()->children[i];
      NodeSchema *&ch = m.schema()->children[i].ptr;
      std::pair<Node, SubSchema *> p;
      p.first = m;
      p.second = &sub_schema;

      /** Name of sub-section */
      std::string cname = ch->get_localized();

      /** Name of sub-schema */
      std::string sname = sub_schema.name.get_id();

      if ((sname.size() > 0) && (ch->name.get_id().compare(sname) != 0))
        cname = sub_schema.name.get_localized();

      if (sub_schema.has_max()) {
        uint32_t nb_max = sub_schema.max;
        if (m.get_children_count(sub_schema.name.get_id()) >= nb_max)
          /* Cannot add more children */
          continue;
      }

      menulist.push_back(
          Gtk::MenuElem(
              langue.get_item("Add ") + cname + "...",
              sigc::bind < std::pair<Node, SubSchema *>
                  > (sigc::mem_fun(parent, &NodeView::add_element), p)));
    }
    parent->popup_menu.popup(event->button, event->time);
    return true;
  }
# endif
  return TreeView::on_button_press_event(event);
  //return return_value;
}

void NodeView::remove_element(Node elt) {
  if (elt.parent().is_nullptr())
    erreur(std::string("Node without parent: ") + elt.schema()->name.get_id());
  else {
    Node parent = elt.parent();
    elt.parent().remove_child(elt);
    populate();
    set_selection(parent);
  }
}

int NodeView::set_selection(Gtk::TreeModel::Row &root, Node reg,
    std::string path) {
  uint32_t cnt = 0;
  typedef Gtk::TreeModel::Children type_children;
  type_children children = root.children();
  for (type_children::iterator iter = children.begin(); iter != children.end();
      ++iter) {
    Gtk::TreeModel::Row row = *iter;
    Node e = row[columns.m_col_ptr];

    std::string chpath = path + ":" + str::int2str(cnt);

    if (e == reg) {
      infos("set_selection: found(2).");

      //void  expand_to_path (const TreeModel::Path& path)
      /*type_children::iterator it1 =
       tree_model.get_iter(   const Path&   path);*/

      //std::string path;
      //path = "0:2";
      tree_view.expand_to_path(Gtk::TreeModel::Path(chpath));
      //tree_view.expand_all();
      tree_view.get_selection()->select(row);

      return 0;
    }
    if (set_selection(row, reg, chpath) == 0)
      return 0;
    cnt++;
  }
  return -1;
}

void NodeView::set_selection(Node reg) {
  uint32_t cnt = 0;
  infos("set_selection(%s)...", reg.get_localized_name().c_str());
  typedef Gtk::TreeModel::Children type_children;
  type_children children = tree_model->children();
  for (type_children::iterator iter = children.begin(); iter != children.end();
      ++iter) {
    std::string path = str::int2str(cnt);
    Gtk::TreeModel::Row row = *iter;
    Node e = row[columns.m_col_ptr];
    if (e == reg) {
      infos("set_selection: found(1).");
      //tree_view.expand_all();
      tree_view.expand_to_path(Gtk::TreeModel::Path(path));
      tree_view.get_selection()->select(row);
      return;
    }

    if (set_selection(row, reg, path) == 0)
      return;
    cnt++;
  }
}

void NodeView::add_element(std::pair<Node, SubSchema *> p) {
  Node m = p.first;
  NodeSchema *&sc = p.second->ptr;
  std::string nname = sc->get_localized();
  Node nv;

  if (sc->has_attribute("name")) {
    std::string title = langue.get_item("Adding new ") + nname
        + langue.get_item(" into ") + m.get_identifier();

    std::string sname = "";
    if (sc->has_attribute("name")) {
      std::string phrase = langue.get_item("Please enter ");
      if(Localized::current_language == Localized::LANG_FR)
        phrase += std::string("le nom du ") + nname + " :";
      else
        phrase += nname + " name:";
      Glib::ustring res = utils::mmi::request_user_string(mainWin, title, phrase);
      if (res == "")
        return;
      sname = res;
    }

    //infos("add_child of type %s..", p.second->name.get_id().c_str());
    nv = Node::create_ram_node(sc);
    if (nv.has_attribute("name"))
      nv.set_attribute("name", sname);
    nv = m.add_child(nv); //p.second->name.get_id());
    populate();
    set_selection(nv);
  } else {
    nv = Node::create_ram_node(sc);
    //model.add_child(schema);
    if (NodeDialog::display_modal(nv) == 0) {
      nv = m.add_child(nv);
      populate();
      set_selection(nv);
    }
  }
  //infos("Now m =\n%s", m.to_xml().c_str());
}

void NodeView::MyTreeView::init(NodeView *parent)
{
  this->parent = parent;
}

NodeView::MyTreeView::MyTreeView()
{
}

NodeView::MyTreeView::MyTreeView(NodeView *parent)/* :
    Gtk::TreeView()*/ {
  init(parent);
}

void NodeView::on_treeview_row_activated(const Gtk::TreeModel::Path &path,
    Gtk::TreeViewColumn *) {
  Gtk::TreeModel::iterator iter = tree_model->get_iter(path);
  if (iter) {
    Gtk::TreeModel::Row row = *iter;
    setup_row_view(row[columns.m_col_ptr]);
  }
}

void NodeView::setup_row_view(Node ptr) {
  Gtk::Widget *widget;

  EVSelectionChangeEvent sce;
  sce.selection = ptr;
  CProvider < EVSelectionChangeEvent > ::dispatch(sce);

  if (!config.display_only_tree) {
    if (rp != nullptr) {
      //infos("Removing prop. contents...");
      properties_frame.remove();
      delete rp;
      rp = nullptr;
    } else {
      properties_frame.remove();
    }

    /*if(ptr == nullptr)
     return;*/

    if (ptr == model) {
      if (only_attributes)
        widget = &table;
      else
        widget = &vbox;
    } else {
      NodeViewConfiguration child_config = config;
      //child_config.show_children = false;
      child_config.disable_all_children = true;
      rp = new NodeView(mainWin, ptr, child_config); //this->show_desc, false, this->small_strings, this->show_separator, nb_columns);
      widget = rp->get_widget();
    }
    properties_frame.set_label(ptr.get_identifier());
    properties_frame.add(*widget);
    properties_frame.show();
    properties_frame.show_all_children(true);
  }
}

Glib::RefPtr<Gdk::Pixbuf> NodeView::get_pics(NodeSchema *&schema)
{
  for (unsigned int i = 0; i < pics.size(); i++) {
    if (pics[i].second == schema) {
      //infos("Returning pic.");
      return pics[i].first;
    }
  }
  throw std::string("pic not found for ") + schema->name.get_id();
}

bool NodeView::has_pic(NodeSchema *&schema) {
  for (unsigned int i = 0; i < pics.size(); i++) {
    if (pics[i].second == schema)
      return true;
  }
  return false;
}

void AttributeListView::update_langue() {
  for (unsigned int i = 0; i < attributes_view.size(); i++) {
    AttributeView *ev = attributes_view[i].av;
    ev->update_langue();
  }
}

bool AttributeListView::is_valid() {
  for (unsigned int i = 0; i < attributes_view.size(); i++) {
    if (!(attributes_view[i].av->is_valid()))
      {
        string s = attributes_view[i].av->model->get_string();
      avertissement("Attribute view[%d] (%s = %s) not valid.", i,
          attributes_view[i].av->model->schema->name.get_id().c_str(),
              s.c_str());
      return false;
    }
  }
  return true;
}

AttributeListView::~AttributeListView() {
  model.remove_listener(this);
  for (unsigned int i = 0; i < attributes_view.size(); i++) {
    delete attributes_view[i].av;
  }
  for (unsigned int i = 0; i < buttons.size(); i++)
    delete buttons[i];
}

void AttributeListView::on_the_realisation() {
  /*infos("realized.");
   Glib::RefPtr<Gdk::Window> wnd;
   wnd = get_window();

   int w, h;
   Gtk::Requisition requi = the_vbox.size_request();
   w = requi.width;
   h = requi.height;
   infos("REQUI w = %d, h = %d.", w, h);

   scroll.set_shadow_type(Gtk::SHADOW_NONE);

   scroll.set_size_request(w + 30, h + 30);*/
}

AttributeListView::AttributeListView()
{
}

void AttributeListView::init(Node model,
    const NodeViewConfiguration &config)
{
  unsigned int natts_total = model.schema()->attributes.size();
  table1.resize(((natts_total > 0) ? natts_total : 1),
      3);
  table1.set_homogeneous(false);


  table2.resize(1, 1);
  table2.set_homogeneous(false);

  //infos("&&&&&&&&&&&&&&&&&&&&&&&& INIT ALV : natts = %d", natts_total);

  this->config = config;
  bool has_no_attributes = true;
  bool indicators_detected = false;
  list_is_sensitive = true;
  this->show_separator = config.show_separator;
  this->small_strings = config.small_strings;
  this->model = model;
  this->show_desc = config.show_desc;
  set_border_width(5);




  /*if(model.schema()->name.has_description())
  {
    auto l = make_description_label(model.schema()->name);
    pack_start(*l, Gtk::PACK_SHRINK);
    //infos("Ajout description node.");
  }*/


  unsigned int nrows = 0;
  // TODO -> refactor (not using "Attribute" class anymore) 
  std::deque<Attribute *> atts; //= model.get_attributes();

  unsigned int natts = 0;
  for(uint32_t i = 0; i < natts_total; i++)
  {
    atts.push_back(model.get_attribute(model.schema()->attributes[i]->name.get_id()));

    Attribute *att = atts[i];

    if(!att->schema->is_hidden)
    {
      natts++;
      nrows++;
      if (show_desc && att->schema->has_description())
      {
        nrows++;
        if(this->show_separator && ((i + 1) < natts_total))
          nrows++; // Séparateur
      }
    }
  }

  if(nrows == 0)
    nrows = 1;
  table1.resize(nrows, 3);

  nb_columns = config.nb_columns;

  /*for (unsigned int i = 0; i < atts.size(); i++) {
    Attribute *att = atts[i];
    if (att->schema->is_hidden)
      natts--;
  }*/

  //if((natts > 10) && (nb_columns == 1))
  //  nb_columns = 2;

  if (((int) natts < (int) config.nb_attributes_by_column) && (nb_columns > 1))
    nb_columns = 1;

  unsigned int row = 0, row_indicators = 0;
  unsigned int first_att = atts.size() + 1;
  unsigned int natt1 = atts.size(), natt2 = 0;

  if (config.show_main_desc && model.schema()->has_description())
  {
    update_description_label(&label_desc, model.schema()->name);

    //label_desc.set_markup(
    //    model.schema()->name.get_description(Localized::LANG_CURRENT));
    the_vbox.pack_start(label_desc, Gtk::PACK_SHRINK);
    Gtk::HSeparator *sep = new Gtk::HSeparator();
    the_vbox.pack_start(*sep, Gtk::PACK_SHRINK, 5 /* padding = 5 pixels */);
  }

  //hbox.pack_start(table1, Gtk::PACK_SHRINK);
  hbox.pack_start(table1, Gtk::PACK_EXPAND_WIDGET);
  //hbox.pack_start(table1, Gtk::PACK_SHRINK);
  if (nb_columns == 2) {
    first_att = (1 + atts.size()) / 2;
    natt1 = first_att;
    natt2 = atts.size() - natt1;
    table1.resize(natt1, 3);
    table2.resize(natt2, 3);
    hbox.pack_start(separator, Gtk::PACK_SHRINK);
    hbox.pack_start(table2, Gtk::PACK_SHRINK);
    infos("n col = 2. n1 = %d, n2 = %d. f = %d.", natt1, natt2, first_att);
  }

  Gtk::Table *ctable = &table1;
  Gtk::Table *otable = nullptr;
  for (unsigned int i = 0; i < atts.size(); i++) 
  {
    unsigned int *crow = &row;

    Attribute *att = atts[i];

    if (ctable == &table_indicators)
      ctable = otable;

    if (i == first_att)
    {
      ctable = &table2;
      *crow = 0;
    }

    if (att->schema->is_hidden)
      continue;

    AttributeView *av = AttributeView::build_attribute_view(att, config, model);

    av->CProvider<KeyPosChangeEvent>::add_listener(this);

    if (att->schema->is_read_only && !att->schema->is_instrument)
    {
      av->set_sensitive(false);
    }
    else if(att->schema->is_read_only)
    {
      av->set_readonly(true);
    }


    if (!indicators_detected && att->schema->is_instrument)
    {
      indicators_detected = true;
      //pack_start(frame_indicators, Gtk::PACK_SHRINK);
      //frame_indicators.set_label(langue.get_item("indicators"));
      //frame_indicators.add(table_indicators);
    }

    if (att->schema->is_instrument)
    {
      if (ctable != &table_indicators)
        otable = ctable;
      ctable = &table_indicators;
      crow = &row_indicators;
    }
    else
      has_no_attributes = false;

    ViewElem ve;
    ve.av = av;
    ve.desc = nullptr;

    //int row_description = *crow + 1;
    for (unsigned int j = 0; j < av->get_nb_widgets(); j++)
    {
      unsigned int next_col = j + 1;
      // Dernier widget de la ligne ?
      if ((j == av->get_nb_widgets() - 1) && (next_col < 3))
      {
        next_col = 3;
        /*if (show_desc && att->schema->has_description() && (next_col == 1))
        {
          //row_description--;
          next_col = 3;
        } else if (next_col == 1)
          next_col = 3;*/
      }
      //infos("x=[%d,%d], y=[%d,%d]", j, next_col, row, row+1);

      float xscale;
      if (j >= 1)
        xscale = 1.0;
      else
        xscale = 0.0;

      Gtk::Alignment *align = new Gtk::Alignment(Gtk::ALIGN_START,
          Gtk::ALIGN_CENTER, xscale, 0);
      align->add(*(av->get_widget(j)));

      //infos("Attach(%d,%d).", j, next_col);
      ctable->attach(*align, j, next_col, *crow, (*crow) + 1,
          Gtk::FILL/* | Gtk::EXPAND*/, Gtk::FILL/* | Gtk::EXPAND*/, 5, 5);

      if (!show_desc && att->schema->has_description())
      {
        av->get_widget(j)->set_has_tooltip();

        std::string desc = att->schema->name.get_description(
            Localized::LANG_CURRENT);
        std::string title = NodeView::mk_label(att->schema->name);

#       ifdef LINUX
        /* Black background on linux */
        std::string color = "#60f0ff";
#       else
        std::string color = "#006080";
#       endif

        std::string tooltip = std::string(
            "<b><span foreground='" + color + "'>") + title
            + std::string("</span></b>\n") + desc;

        if (att->schema->enumerations.size() > 0) {
          for (unsigned int k = 0; k < att->schema->enumerations.size(); k++) {
            Enumeration &e = att->schema->enumerations[k];
            if (e.name.has_description()) {
              tooltip += std::string("\n<b>") + e.name.get_localized()
                  + " : </b>" + e.name.get_description();
            }
          }
        }

        av->get_widget(j)->set_tooltip_markup(tooltip);
      }
      /*else
       {
       erreur("not show tooltip: %s.", att->schema.name.get_id().c_str());
       erreur("desc = <%s>.", att->schema.name.get_description().c_str());
       }*/
    }
    (*crow)++;
    if (show_desc && att->schema->has_description())
    {
      /*auto label_test = make_description_label(att->schema->name);
      Gtk::Window wnd;
      Gtk::VBox vb;
      vb.pack_start(*label_test, Gtk::PACK_SHRINK);
      wnd.add(vb);
      wnd.show_all_children(true);
      Gtk::Main::run(wnd);*/

      auto label = make_description_label(att->schema->name);
      ve.desc = label;

      /*Gtk::Alignment *align = new Gtk::Alignment(Gtk::ALIGN_START,
          Gtk::ALIGN_START, 0, 0);
      align->add(*label);*/

      ctable->attach(*label, 0, 3, *crow, (*crow) + 1,
                     Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK, 5, 5);
      if (att->schema->is_instrument)
        row_indicators++;
      else
        (*crow)++;
      //attach(*label, x0_description, x1_description, row_description, row_description+1);
      //row = row_description; //+ 1;
    }
    if (show_desc && (i < atts.size() - 1) && show_separator && att->schema->has_description())
    {
      Gtk::HSeparator *sep = new Gtk::HSeparator();
      ctable->attach(*sep, 0, 3, (*crow), (*crow) + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
      (*crow)++;
    }
    attributes_view.push_back(ve);
  } // for(i);



  /** Add string lists */

  /*infos("%s has %d children.",
   model.schema()->name.get_id().c_str(),
   model.schema()->children.size());*/

  if (model.schema() != nullptr)
  {
    for (unsigned int i = 0; i < model.schema()->children.size(); i++) 
    {
      SubSchema ss = model.schema()->children[i];

      /*infos("Examining potential tabular %s.",
       ss.to_string().c_str());*/

      if (ss.is_hidden) {
        //verbose("(hidden).");
        continue;
      }

      /* Conditions de bypass des tabulations:
       *
       *
       *
       *
       * if(ss.display_tab || (is_tabular(schema)
       && ((ss.min != 1) || (ss.max != 1))
       && (!ss.display_tree)))



       * Conditons de pass notepad:
       *
       if(is_notebook_display(os)
       && model.has_child(os.name.get_id())
       //&& (!os.is_command)
       && (!os.is_exclusive))
       */

      if ((ss.min == 0) && (ss.max == 1)) {
        //verbose("(optionnal).");
        continue;// Display as optionnal
      }

      NodeSchema *&es = ss.ptr;
      if (es == nullptr)
      {
        erreur("ES is nullptr, for %s (child of %s) !", ss.child_str.c_str(),
            model.schema()->name.get_id().c_str());
        continue;
      }
      if (ss.display_tab
          || (is_tabular(es) && (!ss.display_tree)
              && ((ss.min != 1) || (ss.max != 1)))) 
        {

          /*if (!((is_notebook_display(ss) 
               && model.has_child(ss.name.get_id())
               && (!ss.is_exclusive)))) */
          {

          //infos("Adding tabular for %s...", es->name.get_id().c_str());
          //infos("Root schema is:\n%s\n", model.schema()->to_string().c_str());
          VueTable *slv = new VueTable(model, es, config);
          //table1.attach(*slv, 0, 3, row, row+1, Gtk::FILL, Gtk::FILL, 5, 5);//Gtk::FILL, Gtk::FILL, 5, 5);
          the_vbox.pack_start(/**slv*/*(slv->get_gtk_widget()), Gtk::PACK_EXPAND_WIDGET);
          row++;
          has_no_attributes = false;
        }
      } else 
        {
        //verbose("is_tabular = %s, display_tree = %s.", is_tabular(es) ? "true" : "false", ss.display_tree ? "true" : "false");
      }
    }
  }

  /*infos("%s: resize main(%d), indic(%d)",
   model.schema()->name.get_id().c_str(),
   row,
   row_indicators);*/
  if (row > 0)
  {
    table1.resize(row, 3);
    //trace_verbeuse("Dim table1: %d * %d.", row, 3);
  }
  if (row_indicators > 0)
    table_indicators.resize(row_indicators, 3);

  /*pack_start(frame_att, Gtk::PACK_SHRINK);
   frame_att.set_label(Util::latin_to_utf8(langue.getItem("Attributs")));*/

  /** Add commands */
  
  
  bool has_no_command = true;
# if 0
  NodeSchema *schema = model.schema();
  for (uint32_t i = 0; i < schema->commands.size(); i++) {
    CommandSchema ss = schema->commands[i];
    //if(ss.is_command)
    {
      if (has_no_command) {
        has_no_command = false;

        //trace_major("One action detected.");

        box_actions.set_border_width(5);
        box_actions.set_spacing(5);
        box_actions.set_layout(Gtk::BUTTONBOX_EDGE); //Gtk::BUTTONBOX_END);
        box_actions.set_homogeneous(false);
      }

      /*if(model.get_children_count(ss.name) == 0)
       {
       erreur("No command child.");
       continue;
       }*/

      //NodeSchema *sub_schema = ss.input;
      Gtk::Button *button;
      button = new Gtk::Button();

      //button->set_label(sub_schema->get_localized());
      button->set_label(ss.name.get_localized());

      if (ss.name.has_description()) {
        button->set_tooltip_markup(ss.name.get_description());
        /*Gtk::Label *label = new Gtk::Label();
         label->set_line_wrap(true);
         label->set_justify(Gtk::JUSTIFY_FILL);
         label->set_use_markup(true);
         label->set_markup(Util::latin_to_utf8(sub_schema->description));
         table1.attach(*label, 1, 3, row, row+1, Gtk::FILL, Gtk::FILL, 5, 5);*/
      }

      box_actions.pack_start(*button, Gtk::PACK_SHRINK);

      /*Gtk::Alignment *align = new Gtk::Alignment(Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER, 0, 0);
       align->add(*button);*/

      //table1.attach(*align, 0, 1, row, row+1, Gtk::FILL, Gtk::FILL, 5, 5);
      //row++;
      buttons.push_back(button);

      button->signal_clicked().connect(
          sigc::bind(sigc::mem_fun(*this, &AttributeListView::on_b_command),
              ss.name.get_id()));
    }
  }
# endif

  if (!indicators_detected && has_no_command)
  {
    the_vbox.pack_start(hbox, Gtk::PACK_SHRINK);
  }
  else
  {
    frame_att.add(hbox);
    if (!has_no_attributes)
    {
      the_vbox.pack_start(frame_att, Gtk::PACK_SHRINK);
      frame_att.set_label(langue.get_item("attributes"));
    }
    if (indicators_detected)
    {
      //the_vbox.pack_start(frame_indicators, Gtk::PACK_SHRINK);
      the_vbox.pack_start(table_indicators, Gtk::PACK_SHRINK);
    }
    if (!has_no_command)
    {
      the_vbox.pack_start(/*frame*/box_actions, Gtk::PACK_SHRINK);
    }
  }

  //scroll.add(the_vbox);
  //pack_start(scroll, Gtk::PACK_SHRINK);
  //scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

  pack_start(the_vbox, Gtk::PACK_SHRINK);

  //scroll.set_min_placement_height(500);
  //scroll.set_min_placement_width(500);
  /*int w, h;
   Gtk::Requisition requi = the_vbox.size_request();
   w = requi.width;
   h = requi.height;
   infos("REQUI w = %d, h = %d.", w, h);
   scroll.set_size_request(w + 30, h + 30);*/

  model.add_listener(this);
  ChangeEvent ce;
  on_event(ce);

  //signal_realize().connect(sigc::mem_fun(*this, &AttributeListView::on_the_realisation));
}

AttributeListView::AttributeListView(Node model,
    const NodeViewConfiguration &config)
{
  init(model, config);
}

static void exec_cmde(Node &model, std::string cmde_name)
{
  CommandSchema *cs = model.schema()->get_command(cmde_name);
  if (cs != nullptr) 
  {

    ChangeEvent ce = ChangeEvent::create_command_exec(&model, cs->name.get_id());
    model.dispatch_event(ce);

#   if 0
    /* has parameters ? */
    if (!cs->input.is_nullptr()) {
      //infos("Asking for command parameters..");
      Node prm = Node::create_ram_node(cs->input.get_reference());
      if (NodeDialog::display_modal(prm) == 0) 
      {
        ChangeEvent ce = ChangeEvent::create_command_exec(&model,
            cs->name.get_id(), &prm);
        //infos("Dispatching command + parameters..");
        model.dispatch_event(ce);
        }
      /*ChangeEvent ce = ChangeEvent::create_command_exec(&model,
            cs->name.get_id());
            model.dispatch_event(ce);*/
    }
    /* no parameters */
    else 
    {
      ChangeEvent ce = ChangeEvent::create_command_exec(&model,
          cs->name.get_id());
      model.dispatch_event(ce);
    }
# endif
  }
}

void AttributeListView::on_b_command(std::string name) 
{
  infos("B command detected: %s.", name.c_str());
  CommandSchema *cs = model.schema()->get_command(name);
  if (cs != nullptr) 
  {
    ChangeEvent ce = ChangeEvent::create_command_exec(&model, cs->name.get_id());
    model.dispatch_event(ce);

#   if 0
    /* has parameters ? */
    if (!cs->input.is_nullptr()) 
    {
      infos("Asking for command parameters..");
      Node prm = Node::create_ram_node(cs->input.get_reference());
      if (NodeDialog::display_modal(prm) == 0)
      {
        ChangeEvent ce = ChangeEvent::create_command_exec(&model,
            cs->name.get_id(), &prm);
        infos("Dispatching command + parameters..");
        model.dispatch_event(ce);
      }
    }
    /* no parameters */
    else 
    {
      ChangeEvent ce = ChangeEvent::create_command_exec(&model,
          cs->name.get_id(), nullptr);
      model.dispatch_event(ce);
    }
#   endif
  }
}

void AttributeListView::set_sensitive(bool sensitive)
{
  list_is_sensitive = sensitive;
  ChangeEvent ce;
  on_event(ce);
  /*for(auto i = 0u; i < attributes_view.size(); i++)
  {
    AttributeView *ev = attributes_view[i].av;
    bool valid = model.is_attribute_valid(ev->model->schema->name.get_id());

    bool sens = list_is_sensitive
                && valid
                && !(ev->model->schema->is_read_only && !ev->model->schema->is_instrument);

    ev->set_sensitive(sens);

    //if(ev->model->schema->is_read_only && ev->model->schema->is_instrument)

    if(attributes_view[i].desc != nullptr)
      attributes_view[i].desc->set_sensitive(sens);
  }*/
}

void AttributeListView::on_event(const ChangeEvent &ce) 
{
  //infos(ce.to_string());
  for (unsigned int i = 0; i < attributes_view.size(); i++)
  {
    AttributeView *av = attributes_view[i].av;
    bool valid = model.is_attribute_valid(av->model->schema->name.get_id());
    bool sens = list_is_sensitive && valid && !(av->model->schema->is_read_only && !av->model->schema->is_instrument);
    av->set_sensitive(sens);
    if(attributes_view[i].desc != nullptr)
      attributes_view[i].desc->set_sensitive(sens);
  }
}


SelectionView::SelectionView():
    JFrame(langue.get_item("Options")) 
{
  init();
}

void SelectionView::init()
{
  lock = false;

  scroll.add(tree_view);
  scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  tree_model = Gtk::TreeStore::create(columns);
  tree_view.set_model(tree_model);

  {
    Gtk::CellRendererToggle *renderer_active = Gtk::manage(
        new Gtk::CellRendererToggle());
    renderer_active->signal_toggled().connect(
        sigc::mem_fun(*this, &SelectionView::on_cell_toggled));

    Gtk::TreeView::Column* column = Gtk::manage(
        new Gtk::TreeView::Column(langue.get_item("Use it")));
    column->pack_start(*renderer_active, false);
    column->add_attribute(renderer_active->property_active(),
        columns.m_col_use);
    tree_view.append_column(*column);
  }

  {
    Gtk::CellRendererText *render = Gtk::manage(new Gtk::CellRendererText());
    Gtk::TreeView::Column *viewcol = Gtk::manage(
        new Gtk::TreeView::Column("Option", *render));
    viewcol->add_attribute(render->property_markup(), columns.m_col_name);
    tree_view.append_column(*viewcol);
  }

  {
    Gtk::CellRendererText *render = Gtk::manage(new Gtk::CellRendererText());
    Gtk::TreeView::Column *viewcol = Gtk::manage(
        new Gtk::TreeView::Column("Description", *render));
    viewcol->add_attribute(render->property_markup(), columns.m_col_desc);
    //tree_view.append_column(*viewcol);
  }

  Glib::RefPtr < Gtk::TreeSelection > refTreeSelection =
      tree_view.get_selection();
  refTreeSelection->signal_changed().connect(
      sigc::mem_fun(*this, &SelectionView::on_selection_changed));

  add(scroll);
  scroll.set_size_request(260, 200);
}

void SelectionView::setup(Node model)
{
  this->model = model;
  model.add_listener((CListener<ChangeEvent> *) this);
  update_view();
}

SelectionView::SelectionView(Node model) 
  : JFrame(langue.get_item("Options")) 
{
  init();
  setup(model);
  update_view();
}

void SelectionView::update_view() 
{
  if (!lock) 
  {
    lock = true;
    NodeSchema *selected_schem = get_selected();
    Gtk::TreeModel::Row row;
    clear_table();

    uint32_t nb_optionnals = 0;

    for (unsigned int i = 0; i < model.schema()->children.size(); i++) 
    {
      SubSchema ss = model.schema()->children[i];

      if ((ss.min != 0) || (ss.max != 1))
        continue;

      nb_optionnals++;
    
      NodeSchema *&scheme = ss.ptr;
      std::string nm = ss.name.get_id();
      std::string desc = scheme->name.get_description(Localized::LANG_CURRENT); //scheme->description;

      nm = NodeView::mk_label(scheme->name);

      /*if(langue.has_item(nm))
       nm = langue.getItem(nm);*/
      if (langue.has_item(desc))
        desc = langue.get_item(desc);
      bool in_use = model.has_child(ss.name.get_id());
      row = *(tree_model->append());
      row[columns.m_col_name] = (in_use ? "<b>" : "") + nm
          + (in_use ? "</b>" : "");
      row[columns.m_col_use] = in_use;
      row[columns.m_col_desc] = (!in_use ? "<span color=\"gray\">" : "") + desc
          + (!in_use ? "</span>" : "");
      row[columns.m_col_ptr] = scheme;
      //row[columns.m_col_ptr_opt] = &(model->schema->optionals[i]);
    }

    int dy = 40 + 22 * nb_optionnals;

    if(dy > 350)
      dy = 350;

    //scroll.set_size_request(-1, dy);
    scroll.set_size_request(250, dy);

    tree_view.expand_all();

    // If anything is selected
    if (selected_schem != nullptr)
      set_selection(selected_schem);
    lock = false;
  }
# if 0
  for (unsigned int i = 0; i < model.schema()->optionals.size(); i++)
  {
    NodeSchema *scheme = model.schema()->optionals[i].ptr;
    std::string nm = model.schema()->optionals[i].name;
    std::string desc = scheme->description;

    nm = mk_label2(scheme);

    /*if(langue.has_item(nm))
     nm = langue.getItem(nm);*/
    if(langue.has_item(desc))
    desc = langue.get_item(desc);
    bool in_use = model.has_child(model.schema()->optionals[i].name);
    row = *(tree_model->append());
    row[columns.m_col_name] = (in_use ? "<b>" : "") + nm + (in_use ? "</b>" : "");
    row[columns.m_col_use] = in_use;
    row[columns.m_col_desc] = (!in_use ? "<span color=\"gray\">" : "") + desc + (!in_use ? "</span>" : "");
    row[columns.m_col_ptr] = scheme;
    //row[columns.m_col_ptr_opt] = &(model->schema->optionals[i]);
  }

  scroll.set_size_request(-1, 40 + 22 * model.schema()->optionals.size());

  tree_view.expand_all();

  // If anything is selected
  if(selected_schem != nullptr)
  set_selection(selected_schem);
  lock = false;
}
# endif
}

void SelectionView::clear_table() {
  while (1) {
    typedef Gtk::TreeModel::Children type_children; //minimise code length.
    type_children children = tree_model->children();
    type_children::iterator iter = children.begin();
    if (iter == children.end())
      return;
    Gtk::TreeModel::Row row = *iter;
    tree_model->erase(row);
  }
}

void SelectionView::on_selection_changed() {

}

NodeSchema *SelectionView::get_selected() {
  Glib::RefPtr < Gtk::TreeSelection > refTreeSelection =
      tree_view.get_selection();
  Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
  NodeSchema *selected = nullptr;
  if(iter) 
  {
    Gtk::TreeModel::Row ligne = *iter;
    selected = ligne[columns.m_col_ptr];
  }
  return selected;
}

void SelectionView::set_selection(NodeSchema *&option) {
  if (option != nullptr)
    return;

  typedef Gtk::TreeModel::Children type_children;
  type_children children = tree_model->children();
  for (type_children::iterator iter = children.begin(); iter != children.end();
      ++iter) {
    Gtk::TreeModel::Row row = *iter;
    NodeSchema *opt = row[this->columns.m_col_ptr];
    if(option == opt)
      tree_view.get_selection()->select(row);
  }
}

void SelectionView::on_cell_toggled(const Glib::ustring& path) 
{
  Gtk::TreeModel::iterator iter = tree_model->get_iter(path);

  infos("cell toggled: %s", path.c_str());

  if(iter && !lock) 
  {
    NodeSchema *schem = (*iter)[columns.m_col_ptr];
    string name = schem->name.get_id();

    lock = true;
    (*iter)[columns.m_col_use] = !(*iter)[columns.m_col_use];
    bool in_use = (*iter)[columns.m_col_use];
    if(in_use) 
    {
      infos("Ajout option %s", name.c_str());
      if(!model.has_child(name))
        model.add_child(name);
      else
        infos("Mais déjà présente!");
    } 
    else 
    {
      infos("Retrait option %s", name.c_str());
      if(model.has_child(name))
        model.remove_child(model.get_child(name));
    }
    lock = false;
    //update_view();
  }
}



/******************************************************************
 * STRING LIST VIEW
 ******************************************************************/

VueTable::MyTreeView::MyTreeView(VueTable *parent) :
    Gtk::TreeView() {
  this->parent = parent;
}

bool VueTable::MyTreeView::on_button_press_event(GdkEventButton *event) 
{
  infos("b press event");
  
  if ((event->type == GDK_2BUTTON_PRESS) && (event->button == 1)) {
    Node m = parent->get_selected();
    if (m.is_nullptr()) {
      return TreeView::on_button_press_event(event);
    }
    if (m.schema()->commands.size() > 0) {
      //ChangeEvent ce = ChangeEvent::create_command_exec(Node *source, std::string name, Node *params);
      exec_cmde(m, m.schema()->commands[0].name.get_id());
    }
    return true;
  }
  return TreeView::on_button_press_event(event);
}

Gtk::Widget *VueTable::get_gtk_widget()
{
  return &hbox;//this;
}

VueTable::VueTable(Node &modele_donnees,
                               Node &modele_vue,
                               Controleur *controleur):
        tree_view(this)
{
  nb_lignes = -1;
  Node md = modele_donnees;
  std::string mod = modele_vue.get_attribute_as_string("modele");
  if(mod.size() > 0)
  {
    md = md.get_child(mod);
    if(md.is_nullptr())
    {
      erreur("hughjkjkjk");
      return;
    }
  }
  std::string type_sub = modele_vue.get_attribute_as_string("type-sub");
  if(type_sub.size() == 0)
  {
    erreur("constructeur stringlistview : type-sub non précisé.");
    return;
  }
  auto sub_schema = md.schema()->get_child(type_sub);
  if(sub_schema == nullptr)
  {
    std::string s = md.to_xml();
    erreur("Creation table : sous schema non trouve. Schema = \n%s\ntype sub = %s", s.c_str(), type_sub.c_str());
    return;
  }
  NodeViewConfiguration cfg;
  Config config;

  config.affiche_boutons = modele_vue.get_attribute_as_boolean("affiche-boutons");
  init(md, sub_schema->ptr, cfg, config);
}

VueTable::VueTable(Node model, NodeSchema *&sub,
    const NodeViewConfiguration &cfg):
        tree_view(this)
{
  Config config;
  init(model, sub, cfg, config);
}

void VueTable::init(Node model, NodeSchema *&sub,
    const NodeViewConfiguration &cfg, Config &config)
{
  this->config = config;
  int ncols = 0;
  unsigned int i;

  this->model = model;
  this->schema = sub;
  lock = false;
  std::string name = sub->name.get_localized();

  can_remove = true;
  for (i = 0; i < model.schema()->children.size(); i++) {
    if (model.schema()->children[i].ptr == sub) {
      sub_schema = model.schema()->children[i];
      break;
    }
  }

  if (langue.has_item(name))
    name = langue.get_item(name);

  if ((name[0] >= 'a') && (name[0] <= 'z'))
    name[0] = name[0] + ('A' - 'a');

  name += "s";

  //set_label(name);

  b_add.set_label(langue.get_item("Add..."));
  b_remove.set_label(langue.get_item("Remove"));

  bool has_v_scroll = true;
  bool has_h_scroll = true;

  scroll.set_policy(has_h_scroll ? Gtk::POLICY_AUTOMATIC : Gtk::POLICY_NEVER,
      has_v_scroll ? Gtk::POLICY_AUTOMATIC : Gtk::POLICY_NEVER);
  columns.add(columns.m_col_ptr);
  for (unsigned int i = 0;
      i < schema->attributes.size() + schema->references.size(); i++)
    columns.add(columns.m_cols[i]);

  tree_model = Gtk::TreeStore::create(columns);
  tree_view.set_model(tree_model);

  for (i = 0; i < schema->attributes.size(); i++)
  {
    refptr<AttributeSchema> as = schema->attributes[i];
    std::string cname = as->name.get_localized();

    if (langue.has_item(cname))
      cname = langue.get_item(cname);

    if ((cname[0] >= 'a') && (cname[0] <= 'z'))
      cname[0] += 'A' - 'a';

    if (as->is_hidden)
      continue;

    ncols++;

    if (as->type == TYPE_COLOR)
    {
      ColorCellRenderer2 * const colrenderer = new ColorCellRenderer2(as->constraints);
      Gtk::TreeViewColumn * const colcolumn = new Gtk::TreeViewColumn(cname,
          *Gtk::manage(colrenderer));

      tree_view.append_column(*Gtk::manage(colcolumn));
      colcolumn->add_attribute(colrenderer->property_text(), columns.m_cols[i]);
      colrenderer->property_editable() = true;
      colrenderer->signal_edited().connect(
          sigc::bind < std::string
              > (sigc::mem_fun(*this, &VueTable::on_editing_done), as->name.get_id()));

      if (appli_view_prm.use_touchscreen) {
        colrenderer->signal_editing_started().connect(
            sigc::bind < std::string
                > (sigc::mem_fun(*this, &VueTable::on_editing_start), as->name.get_id()));
      }
    }
    else
    {
      Gtk::CellRendererText *render;
      if(as->enumerations.size() > 0)
      {
        infos("Creation cell combo...");
        auto render_combo = Gtk::manage(new Gtk::CellRendererCombo());
        render = render_combo;

        //render->property_text() = 0;
        render->property_editable() = true;


        struct ModelColonneCombo: public Gtk::TreeModel::ColumnRecord
        {
          Gtk::TreeModelColumn<Glib::ustring> choice; // valeurs possibles
          ModelColonneCombo()
          {
            add(choice);
          }
        };


        ModelColonneCombo col_combo;
        Glib::RefPtr<Gtk::ListStore> model_combo = Gtk::ListStore::create(col_combo);
        for(auto &e : as->enumerations)
        {
          auto s = e.name.get_localized();
          infos("Enumeration : %s", s.c_str());
          (*model_combo->append())[col_combo.choice] = s;
        }

        //Glib::PropertyProxy< bool > prp;
        //prp.set_value(true);

        //render_combo->property_has_entry().set_value(false);
        render_combo->property_model().set_value(model_combo);
        render_combo->property_text_column().set_value(0);
        render_combo->property_has_entry().set_value(false);

        //render_combo->signal_changed().connect(
         //   sigc::bind<std::string>(sigc::mem_fun(*this, &VueTable::on_change), as->name.get_id()));
      }
      else
      {
        render = Gtk::manage(new Gtk::CellRendererText());
        if (as->is_read_only)
        {
          render->property_editable() = false;
        }
        else
        {
          render->property_editable() = true;

          render->signal_editing_started().connect(
              sigc::bind<std::string>(sigc::mem_fun(*this, &VueTable::on_editing_start), as->name.get_id()));
        }
      }
      render->signal_edited().connect(
          sigc::bind<std::string>(sigc::mem_fun(*this, &VueTable::on_editing_done), as->name.get_id()));
      cell_renderers.push_back(render);
      Gtk::TreeView::Column *viewcol = Gtk::manage(
          new Gtk::TreeView::Column(cname, *render));
      viewcol->add_attribute(render->property_markup(), columns.m_cols[i]);
      tree_view.append_column(*viewcol);
    }
  }

  /*for(unsigned int i = 0; i < schema->commands.size(); i++)
   {
   CommandSchema cs = schema->commands[i];
   std::string cname = cs.name.get_localized();
   Gtk::CellRendererToggle *render = Gtk::manage(new Gtk::CellRendererToggle());
   cell_renderers.push_back(render);
   Gtk::TreeView::Column *viewcol = Gtk::manage(new Gtk::TreeView::Column(cname, *render));
   //viewcol->add_attribute(render->property_markup(), columns.m_cols[i]);
   tree_view.append_column(*viewcol);
   }*/

  for (i = 0; i < schema->references.size(); i++)
  {
    RefSchema as = schema->references[i];
    std::string cname = as.name.get_localized();

    if (langue.has_item(cname))
      cname = langue.get_item(cname);

    if ((cname[0] >= 'a') && (cname[0] <= 'z'))
      cname[0] += 'A' - 'a';

    if (as.is_hidden)
      continue;

    ncols++;

    RefCellRenderer * const colrenderer = new RefCellRenderer();
    Gtk::TreeViewColumn * const colcolumn = new Gtk::TreeViewColumn(cname,
        *Gtk::manage(colrenderer));
    tree_view.append_column(*Gtk::manage(colcolumn));
    colcolumn->add_attribute(colrenderer->property_text(),
        columns.m_cols[i + schema->attributes.size()]);
    colrenderer->property_editable() = true;
    colrenderer->signal_edited().connect(
        sigc::bind < std::string
            > (sigc::mem_fun(*this, &VueTable::on_editing_done), as.name.get_id()));
    colrenderer->signal_editing_started().connect(
        sigc::bind < std::string
            > (sigc::mem_fun(*this, &VueTable::on_editing_start), as.name.get_id()));
  }

  Glib::RefPtr < Gtk::TreeSelection > refTreeSelection =
      tree_view.get_selection();
  refTreeSelection->signal_changed().connect(
      sigc::mem_fun(*this, &VueTable::on_selection_changed));

  Gtk::Image *buttonImage_ = new Gtk::Image(
      utils::get_img_path() + files::get_path_separator() + "gtk-go-up16.png");
  b_up.set_image(*buttonImage_);
  buttonImage_ = new Gtk::Image(
      utils::get_img_path() + files::get_path_separator() + "gtk-go-down16.png");
  b_down.set_image(*buttonImage_);

  b_remove.signal_clicked().connect(
      sigc::mem_fun(*this, &VueTable::on_b_remove));
  b_up.signal_clicked().connect(sigc::mem_fun(*this, &VueTable::on_b_up));
  b_down.signal_clicked().connect(
      sigc::mem_fun(*this, &VueTable::on_b_down));
  b_add.signal_clicked().connect(
      sigc::mem_fun(*this, &VueTable::on_b_add));

  /*scroll.add(tree_view);
   hbox.pack_start(scroll, Gtk::PACK_EXPAND_WIDGET);*/

  //hbox.pack_start(tree_view, Gtk::PACK_EXPAND_WIDGET);
  scroll.add(tree_view);
  hbox.pack_start(scroll, Gtk::PACK_EXPAND_WIDGET);

  for (i = 0; i < sub->commands.size(); i++)
  {
    CommandSchema &cs = sub->commands[i];
    // TODO: smart pointer
    Gtk::Button *b = new Gtk::Button();

    button_box.pack_start(*b, Gtk::PACK_SHRINK);

    b->set_label(cs.name.get_localized());
    b->signal_clicked().connect(
        sigc::bind < std::string
            > (sigc::mem_fun(*this, &VueTable::on_b_command), cs.name.get_id()));
  }

  button_box.pack_start(b_up, Gtk::PACK_SHRINK);
  button_box.pack_start(b_down, Gtk::PACK_SHRINK);
  button_box.pack_start(b_add, Gtk::PACK_SHRINK);
  button_box.pack_start(b_remove, Gtk::PACK_SHRINK);

  if (!sub_schema.readonly || (sub->commands.size() > 1))
  {
    if(config.affiche_boutons)
      hbox.pack_start(button_box, Gtk::PACK_SHRINK);
  }
  button_box.set_border_width(5);
  button_box.set_layout(Gtk::BUTTONBOX_END);
  hbox.set_border_width(5);

  //add(hbox);
  update_view();
  //model->CProvider<ChangeEvent>::add_listener(this);
  model.add_listener(this);

  tree_view.set_headers_visible(sub_schema.show_header);

  //set_size_request(130 + ncols * 120, 200);

  //Gtk::Requisition minimum, natural;
  int dx, dy;
  /*tree_view.get_preferred_size(minimum, natural);
   infos("req min = %d,%d, natural = %d,%d.",
   minimum.width, minimum.height,
   natural.width, natural.height);

   int dx = minimum.width, dy = minimum.height;*/
  tree_view.get_size_request(dx, dy);

  //infos("tree view size request = %d,%d.", dx, dy);

  dy = 200;

  if (dx < 150)
    dx = 150;
  if (dx > 300)
    dx = 300;

  if (appli_view_prm.use_touchscreen) {
    dx = 245 + ncols * 120;
    dy = 200;
  } else {
    if (sub->name.get_id().compare("description") == 0) {
      dx = 150 + ncols * 120;
      dy = 150;
    } else {
      dx = 150 + ncols * 120;
      dy = 200;
    }
  }

  if (cfg.table_width != -1)
    dx = cfg.table_width;
  if (cfg.table_height != -1)
    dy = cfg.table_height;

  hbox.set_size_request(dx, dy);

  tree_view.set_grid_lines(Gtk::TREE_VIEW_GRID_LINES_BOTH);

# if 0
  if(appli_view_prm.use_touchscreen)
  set_size_request(245 + ncols * 120, 200);
  else
  {
    if(sub->name.get_id().compare("description") == 0)
    set_size_request(150 + ncols * 120, 150);
    else
    set_size_request(150 + ncols * 120, 200);
  }
# endif
  //set_size_request(-1, 200);
}

void VueTable::on_b_command(std::string command) {
  infos("command detected: %s.", command.c_str());

  Node selection = get_selected();
  exec_cmde(selection, command);
}

void VueTable::update_langue() {
  b_add.set_label(langue.get_item("Add..."));
  b_remove.set_label(langue.get_item("Remove"));
}

void VueTable::on_event(const ChangeEvent &ce) 
{
  if (!lock) 
  {
    //infos(ce.to_string());
    //lock = true;
    //infos("event -> update_view..");
    if(ce.type == ChangeEvent::GROUP_CHANGE)
    {
      auto s = ce.to_string();
      infos("VueTable : change evt: %s.", s.c_str());
      update_view();
    }
    //lock = false;
  }
}

void VueTable::maj_ligne(Gtk::TreeModel::Row &trow, unsigned int row)
{
  auto id_sub = schema->name.get_id();
  Node sub = model.get_child_at(id_sub, row);
  for(auto col = 0u; col < schema->attributes.size(); col++)
  {
    maj_cellule(trow, row, col);
  }
  for (auto j = 0u; j < schema->references.size(); j++)
  {
    Node ref = sub.get_reference(schema->references[j].name.get_id());
    //ref.infos("one ref.");
    std::string name = ref.get_localized().get_localized();
    trow[columns.m_cols[schema->attributes.size() + j]] = name;
  }
}

void VueTable::maj_cellule(Gtk::TreeModel::Row &trow, unsigned int row, unsigned int col)
{
  auto id_sub = schema->name.get_id();
  Node sub = model.get_child_at(id_sub, row);
  std::string val, name;
  auto &as = schema->attributes[col];

  name = as->name.get_id();
  val = as->get_ihm_value(sub.get_attribute_as_string(name));

  if(as->has_unit())
    val += " " + as->unit;

  trow[columns.m_cols[col]] = val;
}

//int essai_plante = 0;

void VueTable::update_view() 
{
  uint32_t i;

  //if(essai_plante)
    //erreur("Vue table : update view innatendu.");

  if (!lock)
  {
    infos("VueTable -> maj.");

    auto id_sub = schema->name.get_id();

    if((nb_lignes > 0) && ((int) model.get_children_count(id_sub) == nb_lignes))
    {
      infos("VueTable : maj slmt des valeurs.");
      lock = true;
      typedef Gtk::TreeModel::Children type_children;
      type_children children = tree_model->children();
      auto iter = children.begin();
      // Only update the values
      for(i = 0; i < (unsigned int) nb_lignes; i++)
      {
        auto r = *iter++;
        maj_ligne(r, i);
      }
      lock = false;
      return;
    }

      can_remove = true;
      lock = true;
      Node selected = get_selected();
      Node first, last;
      Gtk::TreeModel::Row row;
      clear_table();

      for (i = 0; i < model.get_children_count(id_sub); i++)
      {
        Node sub = model.get_child_at(id_sub, i);
        row = *(tree_model->append());

        maj_ligne(row, i);

        row[columns.m_col_ptr] = sub;
      }

      //scroll.set_size_request(-1, 40 + 22 * model->schema->optionals.size());

      tree_view.expand_all();

      // If anything is selected
      if (!selected.is_nullptr()) {
        set_selection(selected);
        b_remove.set_sensitive(true);

        b_up.set_sensitive(selected != first);
        b_down.set_sensitive(selected != last);
      } else {
        b_up.set_sensitive(false);
        b_down.set_sensitive(false);
        b_remove.set_sensitive(false);
      }

      b_add.set_sensitive(true);

      int nchild = model.get_children_count(this->schema->name.get_id());
      for (uint32_t i = 0; i < model.schema()->children.size(); i++) {
        SubSchema ss = model.schema()->children[i];
        if (ss.ptr == schema) {
          if (ss.has_min() && (nchild <= ss.min)) {
            b_remove.set_sensitive(false);
            can_remove = false;
          }
          if (ss.has_max() && (nchild >= ss.max))
            b_add.set_sensitive(false);
          break;
        }
      }
      lock = false;
    }

  if (sub_schema.readonly) {
    b_add.set_sensitive(false);
    b_down.set_sensitive(false);
    b_up.set_sensitive(false);
    b_remove.set_sensitive(false);
  }
}

void VueTable::clear_table() {
  while (1) {
    typedef Gtk::TreeModel::Children type_children;
    type_children children = tree_model->children();
    type_children::iterator iter = children.begin();
    if (iter == children.end())
      return;
    Gtk::TreeModel::Row row = *iter;
    tree_model->erase(row);
  }
}

void VueTable::on_selection_changed() {
  //infos("selection changed.");
  if (!lock) {
    lock = true;

    Node selected = get_selected();
    if (selected.is_nullptr()) {
      b_remove.set_sensitive(false);
      b_up.set_sensitive(false);
      b_down.set_sensitive(false);
    } else {
      b_remove.set_sensitive(can_remove);
      unsigned int n = model.get_children_count(sub_schema.child_str);
      Node first = model.get_child_at(sub_schema.child_str, 0);
      Node last = model.get_child_at(sub_schema.child_str, n - 1);
      b_up.set_sensitive(selected != first);
      b_down.set_sensitive(selected != last);
    }

    lock = false;
  }
}

Node VueTable::get_selected() {
  Glib::RefPtr < Gtk::TreeSelection > refTreeSelection =
      tree_view.get_selection();
  Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
  Node selected;
  if (iter) {
    Gtk::TreeModel::Row ligne = *iter;
    selected = ligne[columns.m_col_ptr];
  }
  return selected;
}

void VueTable::set_selection(Node sub) {
  if (sub.is_nullptr())
    return;

  typedef Gtk::TreeModel::Children type_children;
  type_children children = tree_model->children();
  for (type_children::iterator iter = children.begin(); iter != children.end();
      ++iter) {
    Gtk::TreeModel::Row row = *iter;
    Node e = row[columns.m_col_ptr];
    if (e == sub)
      tree_view.get_selection()->select(row);
  }
}

void VueTable::on_editing_done(Glib::ustring path, 
				     Glib::ustring text,
				     std::string col) 
{
  infos("edit done.");

  bool display_editing_dialog = false;

  if (appli_view_prm.use_touchscreen)
    display_editing_dialog = true;

  // Check if a column is a text view
  for (unsigned int i = 0; i < schema->attributes.size(); i++)
  {
    refptr<AttributeSchema> &as = schema->attributes[i];
    if ((as->type == TYPE_STRING) && (as->formatted_text))
    {
      display_editing_dialog = true;
      break;
    }
  }

  if (display_editing_dialog)
    return;

  //if(appli_view_prm.use_touchscreen)
  //  return;

  if (!lock) {
    lock = true;

    std::string p = path;
    std::string s = text;

    //infos("slview : update val s = %s.", s.c_str());

    int row = atoi(p.c_str());

    infos("Editing done: col=%s, row=%d, path=%s, val=%s.", 
	  col.c_str(), row,
	  p.c_str(), s.c_str());

    if ((row < 0)
        || (row >= (int) model.get_children_count(schema->name.get_id()))) {
      erreur("invalid row.");
      return;
    }
    Node sub = model.get_child_at(schema->name.get_id(), row);

    lock = false;
    if (sub.has_attribute(col))
      sub.set_attribute(col, s);
    update_view();
  }
}

void VueTable::on_editing_start(Gtk::CellEditable *ed, Glib::ustring path,
    std::string col) {
  infos("editing start: col = %s", col.c_str());
  for (unsigned int i = 0; i < schema->references.size(); i++) {
    if (schema->references[i].name.get_id().compare(col) == 0) {
      infos("It is a reference column.");
      RefCellEditable *rce = (RefCellEditable *) ed;

      std::string p = path;
      int row = atoi(p.c_str());

      if ((row < 0)
          || (row >= (int) model.get_children_count(schema->name.get_id()))) {
        erreur("invalid row.");
        return;
      }
      Node sub = model.get_child_at(schema->name.get_id(), row);
      //sub.infos("setup model..");
      rce->setup_model(sub, col);
      return;
    }
  }

  bool display_editing_dialog = false;

  if (appli_view_prm.use_touchscreen)
    display_editing_dialog = true;

  // Check if a column is a text view
  for (unsigned int i = 0; i < schema->attributes.size(); i++) {
    refptr<AttributeSchema> &as = schema->attributes[i];
    if ((as->type == TYPE_STRING) && (as->formatted_text)) {
      display_editing_dialog = true;
      break;
    }
  }

  if (!display_editing_dialog)
    return;

  if (!lock) {
    lock = true;
    std::string p = path;

    int row = atoi(p.c_str());

    infos("Editing start: col=%s, row=%d, path=%s.", col.c_str(), row,
        p.c_str());

    if ((row < 0)
        || (row >= (int) model.get_children_count(schema->name.get_id()))) {
      erreur("invalid row.");
      return;
    }
    Node sub = model.get_child_at(schema->name.get_id(), row);

    NodeDialog::display_modal(sub);

    for (unsigned int i = 0; i < cell_renderers.size(); i++) {
      cell_renderers[i]->stop_editing(false);
    }

    tree_view.get_selection()->unselect_all();
    //GtkKeyboard::get_instance()->present();

    lock = false;

    update_view();
  }
}

bool VueTable::is_valid() {
  return true;
}

void VueTable::on_b_remove() {
  Node sub = get_selected();
  model.remove_child(sub);
  update_view();
}

void VueTable::on_b_up() {
  Node sub = get_selected();
  set_selection(sub.parent().up(sub));
}

void VueTable::on_b_down() {
  Node sub = get_selected();
  set_selection(sub.parent().down(sub));
}

void VueTable::on_b_add() {
  Node nv = model.add_child(schema);
  if (NodeDialog::display_modal(nv))
    model.remove_child(nv);
  else {
    update_view();
    set_selection(nv);
  }
}

NodeDialog::~NodeDialog() {
  window->hide();
  delete ev;
}

void NodeDialog::maj_langue()
{
  if(ev != nullptr)
    ev->maj_langue();
  tool_valid.set_label(langue.get_item("b-valid"));
  tool_cancel.set_label(langue.get_item("b-cancel"));
  auto titre = model.schema()->name.get_localized();
  wnd.set_title(titre);
  dlg.set_title(titre);
  label_title.set_markup(titre);
  b_apply.set_label(langue.get_item("b-apply"));
  b_valid.set_label(langue.get_item("b-valid"));
  b_close.set_label(langue.get_item("b-cancel"));
}

NodeDialog::NodeDialog(Node model, bool modal, Gtk::Window *parent_window) :
    dlg("", modal), keyboard(&dlg), kb_align(Gtk::ALIGN_CENTER,
        Gtk::ALIGN_CENTER, 0, 0) {
  this->modal = modal;
  this->fullscreen = false;
  lastx = -1;
  lasty = -1;
  exposed = false;


  //infos("CONS: modal = %s.", modal ? "true" : "false");

  lock = false;
  u2date = true;
  this->model = model;
  backup = Node::create_ram_node(model.schema());
  backup.copy_from(model);

  NodeViewConfiguration config;
  config.show_desc = false;
  config.show_children = false;
  ev = new NodeView(mainWindow, backup, config);

  ev->CProvider<KeyPosChangeEvent>::add_listener(this);

  //trace_major("EV IS READY.");


  if(!modal)
  {
    window = &wnd;
    wnd.set_position(Gtk::WIN_POS_CENTER);
    wnd.add(vbox);
    vb = &vbox;
  }
  else
  {
    window = &dlg;
    dlg.set_position(Gtk::WIN_POS_CENTER);
    vb = dlg.get_vbox();
  }

  if (!appli_view_prm.use_decorations) 
  {
    window->set_decorated(false);
    vb->pack_start(label_title, Gtk::PACK_SHRINK);
  }

  /*vb->pack_start(scroll, Gtk::PACK_EXPAND_WIDGET);
   scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   scroll.add(*(ev->get_widget()));*/

  vb->pack_start(*(ev->get_widget()), Gtk::PACK_SHRINK);

  Gtk::Image *img_valid, *img_cancel, *img_apply;
  if (appli_view_prm.img_validate.size() > 0)
    img_valid = new Gtk::Image(appli_view_prm.img_validate);
  else
    img_valid = new Gtk::Image(Gtk::StockID(Gtk::Stock::APPLY),
        Gtk::IconSize(Gtk::ICON_SIZE_BUTTON));

  if (appli_view_prm.img_cancel.size() > 0)
    img_cancel = new Gtk::Image(appli_view_prm.img_cancel);
  else
    img_cancel = new Gtk::Image(Gtk::StockID(Gtk::Stock::CANCEL),
        Gtk::IconSize(Gtk::ICON_SIZE_BUTTON));

  img_apply = new Gtk::Image(Gtk::StockID(Gtk::Stock::APPLY),
      Gtk::IconSize(Gtk::ICON_SIZE_BUTTON));

  if (appli_view_prm.use_button_toolbar)
  {
    tool_valid.set_icon_widget(*img_valid);
    tool_cancel.set_icon_widget(*img_cancel);
    toolbar.insert(tool_cancel, -1,
        sigc::mem_fun(*this, &NodeDialog::on_b_close));
    toolbar.insert(sep2, -1);
    sep2.set_expand(true);
    sep2.set_property("draw", false);
    toolbar.insert(tool_valid, -1,
        sigc::mem_fun(*this, &NodeDialog::on_b_apply));


  }
  else
  {
    hbox.pack_end(b_close, Gtk::PACK_SHRINK);
    hbox.pack_end(b_apply, Gtk::PACK_SHRINK);
    hbox.pack_end(b_valid, Gtk::PACK_SHRINK);
    hbox.set_layout(Gtk::BUTTONBOX_END);

    b_close.set_border_width(4);
    b_apply.set_border_width(4);
    b_valid.set_border_width(4);

    b_valid.set_image(*img_valid);
    b_apply.set_image(*img_apply);
    b_close.set_image(*img_cancel);
  }

  if (appli_view_prm.use_touchscreen) {
    keyboard.target_window = window;
    kb_align.add(keyboard);
  }

  maj_langue();

  add_widgets();
  /*
   if(appli_view_prm.vkeyboard_below)
   {
   if(appli_view_prm.use_button_toolbar)
   vb->pack_start(toolbar, Gtk::PACK_SHRINK);
   else
   vb->pack_start(hbox, Gtk::PACK_SHRINK);
   vb->pack_start(keyboard_separator, Gtk::PACK_SHRINK);
   vb->pack_start(keyboard, Gtk::PACK_SHRINK);
   }
   else
   {
   vb->pack_start(keyboard_separator, Gtk::PACK_SHRINK);
   vb->pack_start(keyboard, Gtk::PACK_SHRINK);
   if(appli_view_prm.use_button_toolbar)
   vb->pack_start(toolbar, Gtk::PACK_SHRINK);
   else
   vb->pack_start(hbox, Gtk::PACK_SHRINK);
   }
   }
   else
   {
   if(appli_view_prm.use_button_toolbar)
   vb->pack_start(toolbar, Gtk::PACK_SHRINK);
   else
   vb->pack_start(hbox, Gtk::PACK_SHRINK);
   }*/

  window->show_all_children(true);

  b_close_ptr = &b_close;
  b_apply_ptr = &b_apply;

  if (modal) {
    if (parent_window != nullptr)
      dlg.set_transient_for(*parent_window);
    else if (mainWindow != nullptr)
      dlg.set_transient_for(*mainWindow);
  } else {
    wnd.show();
  }

  b_valid.signal_clicked().connect(
      sigc::mem_fun(*this, &NodeDialog::on_b_valid));
  b_apply.signal_clicked().connect(
      sigc::mem_fun(*this, &NodeDialog::on_b_apply));
  b_close.signal_clicked().connect(
      sigc::mem_fun(*this, &NodeDialog::on_b_close));

  update_view();
  backup.add_listener(this);

  window->signal_draw().connect(
      sigc::mem_fun(*this, &NodeDialog::on_expose_event2));

  //infos("done cons.");
}

void NodeDialog::on_b_close() {
  result_ok = false;
  window->hide();
  /*if(modal)
   {
   dlg.hide();
   }
   else
   {
   wnd.hide();
   //if(vkb_displayed)
   //GtkKeyboard::get_instance()->close();
   }*/
}

#if 0
void NodeDialog::on_b_valid()
{
  model.copy_from(backup);
  u2date = true;
  update_view();

  wnd.hide();
  if(vkb_displayed)
  GtkKeyboard::get_instance()->close();

  /*NodeChangeEvent ce;
   ce.source = model;
   dispatch(ce);*/
}
#endif

void NodeDialog::on_b_apply()
{
  model.copy_from(backup);
  u2date = true;
  update_view();
  NodeChangeEvent ce;
  ce.source = model;
  dispatch(ce);
}

void NodeDialog::on_b_valid()
{
  model.copy_from(backup);
  u2date = true;
  update_view();

  infos("On b apply.");

  /*if(pseudo_dialog)
   {
   infos("& pseudo dialog.");
   if(vkb_displayed)
   GtkKeyboard::get_instance()->close();
   result_ok = true;
   wnd.hide();
   }
   else*/
  {
    NodeChangeEvent ce;
    ce.source = model;
    dispatch(ce);
  }

  result_ok = true;
  window->hide();
  /*if(modal)
   {
   dlg.hide();
   }
   else
   {
   wnd.hide();
   //if(vkb_displayed)
   //GtkKeyboard::get_instance()->close();
   }*/
}

bool NodeDialog::on_expose_event2(const Cairo::RefPtr<Cairo::Context> &cr)
{
  if (!lock)
  {
    lock = true;
    int x = window->get_allocation().get_width();
    int y = window->get_allocation().get_height();

    if (/*exposed &&*/((x != (int) lastx) || (y != (int) lasty))) {
      trace_verbeuse("Size of node-dialog changed: %d,%d -> %d,%d.", lastx, lasty, x,
          y);
      DialogManager::setup_window(this, fullscreen);
    }

    lastx = x;
    lasty = y;
    lock = false;
  }
  exposed = true;
  return true;
}

void NodeDialog::update_view() {
  bool val = ev->is_valid();

  infos("update view..");

  //infos("update view: u2date = %s, pseudo = %s, is_valid = %s.", u2date ? "true" : "false", pseudo_dialog ? "true" : "false", val ? "true" : "false");
# if 0
  if(u2date)
  {
    //b_apply.set_sensitive(/*false*/pseudo_dialog);
    b_apply_ptr->set_sensitive(/*false*/pseudo_dialog);
  }
  else
# endif
  {
    //infos("Is valid = %s.", val ? "true" : "false");
    b_apply_ptr->set_sensitive(val);

    tool_valid.set_sensitive(val);
  }
  //infos("update view done.");
}

static std::vector<NodeDialog *> dial_instances;

void NodeDialog::on_event(const ChangeEvent &ce) {
  infos("change event...");
  if (!lock) {
    lock = true;
    u2date = false;
    update_view();
    lock = false;
  }
  if (ce.type == ChangeEvent::COMMAND_EXECUTED) 
    {
    /* Reroute the change source from the backup model to the original model */
    /*XPath path;
    backup.get_path_to(*(ce.source_node), path);
    Node owner;
    owner = model.get_child(path);
    ChangeEvent ce2 = ChangeEvent::create_command_exec(&owner, ce.path.get_last(),
                                                       ce.cmd_params);
                                                       this->model.dispatch_event(ce2);*/
  }
  infos("done.");
}

Gtk::Window *NodeDialog::get_window() {
  return window;
}

void NodeDialog::unforce_scroll() {
  infos("Unforce scroll..");
  scroll.remove();
  vb->remove(scroll);
  remove_widgets();

  vb->pack_start(*(ev->get_widget()), Gtk::PACK_SHRINK);

  add_widgets();

  window->show_all_children(true);
}

void NodeDialog::remove_widgets() {
  if (appli_view_prm.use_touchscreen) {
    vb->remove(keyboard_separator);
    vb->remove(kb_align);
  }
  if (appli_view_prm.use_button_toolbar)
    vb->remove(toolbar);
  else
    vb->remove(hbox);
}

void NodeDialog::add_widgets()
{
  if (appli_view_prm.use_touchscreen)
  {
    if (appli_view_prm.vkeyboard_below)
    {
      if (appli_view_prm.use_button_toolbar)
        vb->pack_start(kb_align, Gtk::PACK_SHRINK);
      else
        vb->pack_start(hbox, Gtk::PACK_SHRINK);
      vb->pack_start(keyboard_separator, Gtk::PACK_SHRINK);
      vb->pack_start(keyboard, Gtk::PACK_SHRINK);
    }
    else
    {
      vb->pack_start(keyboard_separator, Gtk::PACK_SHRINK);
      vb->pack_start(kb_align, Gtk::PACK_SHRINK);
      if (appli_view_prm.use_button_toolbar)
        vb->pack_start(toolbar, Gtk::PACK_SHRINK);
      else
        vb->pack_start(hbox, Gtk::PACK_SHRINK);
    }
  } else {
    if (appli_view_prm.use_button_toolbar)
      vb->pack_start(toolbar, Gtk::PACK_SHRINK);
    else
      vb->pack_start(hbox, Gtk::PACK_SHRINK);
  }
}



void NodeDialog::force_scroll(int dx, int dy) {
  infos("Force scroll(%d,%d)..", dx, dy);
  vb->remove(*(ev->get_widget()));
  remove_widgets();

  vb->pack_start(scroll, Gtk::PACK_EXPAND_WIDGET);
  scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  scroll.add(*(ev->get_widget()));

  add_widgets();

  window->show_all_children(true);
}

void NodeDialog::on_event(const KeyPosChangeEvent &kpce) {
  if (appli_view_prm.use_touchscreen) {
    infos("update kb valid chars...");
    keyboard.set_valid_chars(kpce.vchars);
  }
}

NodeDialog *NodeDialog::display(Node model) {
  for (unsigned int i = 0; i < dial_instances.size(); i++)
  {
    if (dial_instances[i]->model == model)
    {
      //model.infos("show old window.");
      dial_instances[i]->backup.copy_from(model);
      dial_instances[i]->wnd.show();
      return dial_instances[i];
    }
  }
  NodeDialog *nv = new NodeDialog(model, false);
  nv->fullscreen = false;
  dial_instances.push_back(nv);
  DialogManager::setup_window(nv);
  //nv->vkb_displayed = false;
  nv->wnd.show();
  /*if(appli_view_prm.use_touchscreen)
   {
   GtkKeyboard *kb = GtkKeyboard::get_instance();
   kb->display(&nv->wnd);
   nv->vkb_displayed = true;
   }*/
  return nv;
}

int NodeDialog::display_modal(Node model, bool fullscreen,
    Gtk::Window *parent_window) {
  NodeDialog *nv = new NodeDialog(model, true, parent_window);
  nv->fullscreen = fullscreen;
  DialogManager::setup_window(nv, fullscreen);
  nv->result_ok = false;
  infos("run..");
  int res = nv->dlg.run();

  //int res = 0;
  //Gtk::Main::run(nv->dlg);
  infos("run done.");
  infos("hide...");
  nv->dlg.hide();

  if ((res == Gtk::RESPONSE_ACCEPT) || (nv->result_ok)) {
    infos("copy model..");
    model.copy_from(nv->backup);
    infos("delete dialog..");
    nv->lock = true;
    delete nv;
    infos("done.");
    return 0;
  }
  delete nv;
  return 1;
# if 0
  if(appli_view_prm.use_touchscreen)
  {
    NodeDialog *nv = new NodeDialog(model, false, false, true);
    //GtkKeyboard *kb = GtkKeyboard::get_instance();

    nv->vkb_displayed = true;

    DialogManager::get_instance()->setup(&(nv->wnd));

    nv->wnd.signal_focus_in_event().connect(sigc::mem_fun(*nv, &NodeDialog::on_focus_in));
    nv->wnd.signal_focus_out_event().connect(sigc::mem_fun(*nv, &NodeDialog::on_focus_out));

    nv->infos("running wnd..");
    nv->result_ok = false;

    Gtk::Main::run(nv->wnd);
    nv->infos("gtk return.");
    //int res = 0;
    DialogManager::get_instance()->dispose();
    //GtkKeyboard::get_instance()->close();
    nv->wnd.hide();

    //if(res == Gtk::RESPONSE_ACCEPT)
    if(nv->result_ok)
    {
      model.copy_from(nv->backup);
      delete nv;
      return 0;
    }
    delete nv;
    return 1;
  }
  else
  {
    NodeDialog *nv = new NodeDialog(model, true, true);
    int res = nv->dlg.run();
    nv->dlg.hide();
    if(res == Gtk::RESPONSE_ACCEPT)
    {
      model.copy_from(nv->backup);
      delete nv;
      return 0;
    }
    delete nv;
    return 1;
  }
# endif
}




/*******************************************************************
 *               CHOICE VIEW IMPLEMENTATION                        *
 *******************************************************************/
VueChoix::VueChoix(Attribute *model, Node parent,
    NodeViewConfiguration config) {
  tp = "choice";
  lock = false;
  this->model = model;
  this->config = config;
  model_parent = parent;
  current_view = nullptr;

  //parent.infos("parent.");
  if (model->schema->enumerations.size() == 0) {
    avertissement("no enumerations ?");
  }
  //infos("Schema = '%s'.", model->schema.enumerations[0].schema->name.get_id().c_str());

  nb_choices = model->schema->enumerations.size();
  radios = (Gtk::RadioButton **) malloc(
      nb_choices * sizeof(Gtk::RadioButton *));

  for (uint32_t j = 0; j < nb_choices; j++) 
  {
    if (j == 0) 
    {
      radios[j] = new Gtk::RadioButton();
      group = radios[j]->get_group();
    } 
    else
      radios[j] = new Gtk::RadioButton(group);

    //NodeSchema *sub_schema = model->schema.enumerations[j].schema;
    infos(model->schema->name.get_id());
    //infos("j = %d.", j);

    radios[j]->set_label(model->schema->enumerations[j].name.get_localized());

    //if (sub_schema != nullptr) 
    {
      //radios[j]->set_label(sub_schema->get_localized());
      vbox.pack_start(*radios[j], Gtk::PACK_SHRINK);

      radios[j]->signal_toggled().connect(
            sigc::bind(sigc::mem_fun(*this, &VueChoix::on_radio_activate), j));

      //radios[j]->signal_toggled().connect(
      //    sigc::mem_fun(*this, &ChoiceView::on_radio_activate));
    }

    radios[j]->signal_focus_in_event().connect(
        sigc::mem_fun(*this, &AttributeView::on_focus_in));
  }

  frame.add(vbox);

  update_langue();
  update_sub_view();
  ChangeEvent ce;
  on_event(ce);
}

VueChoix::~VueChoix() {
  //infos("~ChoiceView(), model = %x...", (uint32_t) model);
  lock = true;
  if (this->current_view != nullptr) {
    vbox.remove(*(current_view->get_widget()));
    delete current_view;
    current_view = nullptr;
  }
  for (uint32_t j = 0; j < nb_choices; j++) {
    vbox.remove(*(radios[j]));
    delete radios[j];
  }
  free(radios);
  //infos("~ChoiceView() done.");
}

void VueChoix::on_radio_activate(unsigned int num)
{
  //uint32_t j;

  //infos("on radio activate.");

  if (!lock) {
    //lock = true;

    if(radios[num]->get_active())
    {
      model->set_value(model->schema->enumerations[num].value);
    }

    /* get active radio */
    /*for (j = 0; j < nb_choices; j++)
    {
      //NodeSchema *sub_schema = model->schema.enumerations[j].schema;
      if (radios[j]->get_active())
      {
        model->set_value(model->schema->enumerations[j].value);
        break;
      }
    }
    if (j == nb_choices)
      erreur("choice view: none selected.");*/

    update_sub_view();
    //lock = false;
  }
}

void VueChoix::update_langue() {
  bool old_lock = lock;

  lock = true;

  for (uint32_t j = 0; j < nb_choices; j++) 
  {
    //NodeSchema *sub_schema = model->schema.enumerations[j].schema;
    //if (sub_schema != nullptr)
    //radios[j]->set_label(sub_schema->get_localized());
    //else
      radios[j]->set_label(model->schema->enumerations[j].name.get_localized());
  }

  frame.set_label(NodeView::mk_label(model->schema->name));

  if (current_view != nullptr)
    current_view->update_langue();

  lock = old_lock;
}

unsigned int VueChoix::get_nb_widgets() {
  return 1;
}

Gtk::Widget *VueChoix::get_widget(int index) {
  return &frame;
}

Gtk::Widget *VueChoix::get_gtk_widget()
{
  return &frame;
}

void VueChoix::set_sensitive(bool b) {
  for (uint32_t j = 0; j < nb_choices; j++)
    radios[j]->set_sensitive(b);
  if (current_view != nullptr)
    current_view->set_sensitive(b);
}

void VueChoix::on_event(const ChangeEvent &ce) {
  if (!lock) {
    lock = true;

    unsigned int val = model->get_int();

    if (val > nb_choices)
      erreur("Invalid value: %d (n choices = %d).", val, nb_choices);
    else
    {
      radios[val]->set_active(true);
      for(auto i = 0u; i < nb_choices; i++)
        if(i != val)
          radios[i]->set_active(false);
    }
    update_sub_view();
    lock = false;
  }
}

bool VueChoix::is_valid() {
  if (current_view == nullptr)
    return true;
  return current_view->is_valid();
}

void VueChoix::update_sub_view() 
{
  //infos("update_sub_view...");

  if (current_view != nullptr) 
  {
    vbox.remove(*(current_view->get_widget()));
    delete current_view;
    current_view = nullptr;
  }

  int model_val = model->get_int();
  int nb_enums = model->schema->enumerations.size();

  if (model_val >= nb_enums) 
  {
    erreur("Model value = %d > number of enums = %d.", model_val, nb_enums);
    return;
  }


  NodeSchema *specific_schema = model->schema->enumerations[model_val].schema;

  if(specific_schema != nullptr)
  {
    std::string sname = specific_schema->name.get_id();

    if (!model_parent.has_child(sname)) 
    {
      erreur("No child of type %s.", sname.c_str());
      return;
    }

    Node sub = model_parent.get_child(sname);

    if(sub.is_nullptr())
    {
      erreur("no such child: %s", sname.c_str());
    }
    else
    {
      current_view = new NodeView(mainWindow, sub, config);
      current_view->CProvider < KeyPosChangeEvent > ::add_listener(this);
      vbox.pack_start(*current_view->get_widget(), Gtk::PACK_SHRINK);
    }
  }
  vbox.show_all_children(true);
}

/*******************************************************************
 *               LED VIEW IMPLEMENTATION                       *
 *******************************************************************/
VueLed::~VueLed()
{

}

VueLed::VueLed(Attribute *model, bool editable, bool error)
{
  lock = false;
  this->model = model;
  this->editable = editable;
  lab.set_use_markup(true);
  std::string s = NodeView::mk_label(model->schema->name);
  update_langue();
  led.set_mutable(!model->schema->is_read_only && editable);
  led.set_red(model->schema->is_error || error);
  led.light(model->get_boolean());
  led.add_listener(this, &VueLed::on_signal_toggled);
  led.show();
}

void VueLed::update_langue() 
{
  lab.set_markup("<b>" + NodeView::mk_label_colon(model->schema->name) + "</b>");
}

unsigned int VueLed::get_nb_widgets()
{
  return 2;
}

Gtk::Widget *VueLed::get_widget(int index)
{
  if(index == 0)
    return &lab;
  else
    return &led;
}

Gtk::Widget *VueLed::get_gtk_widget()
{
  return &led;
}

void VueLed::set_readonly(bool b)
{
  led.set_mutable(!b);
}

void VueLed::set_sensitive(bool b) {
  led.set_mutable(b && !model->schema->is_read_only);
  lab.set_sensitive(b);
  led.set_sensitive(b);
}

void VueLed::on_signal_toggled(const LedEvent &le) {
  if (!lock) {
    lock = true;
    model->set_value(led.is_on());
    lock = false;
  }
}

void VueLed::on_event(const ChangeEvent &ce) {
  if (!lock) {
    lock = true;
    led.light(model->get_boolean());
    lock = false;
  }
}

/*******************************************************************
 *******************************************************************
 *               GENERIC VIEW                                      *
 *******************************************************************
 *******************************************************************/

VueGenerique::VueGenerique()
{
  is_sensitive = true;
}

void VueGenerique::set_sensitive(bool sensitive)
{
  is_sensitive = sensitive;
  for(auto &e: enfants)
  {
    if(e != nullptr)
      e->set_sensitive(sensitive);
  }
  get_gtk_widget()->set_sensitive(sensitive);
}

VueGenerique::~VueGenerique()
{
  for(auto e: enfants)
  {
    if(e != nullptr)
      delete e;
  }
  enfants.clear();
}

void VueGenerique::maj_contenu_dynamique()
{
  infos(" ++++ GenericView::maj_contenu_dynamique ++++");
  for(auto &e: enfants)
  {
    if(e != nullptr)
      e->maj_contenu_dynamique();
  }
}


VueGenerique::WidgetType VueGenerique::desc_vers_type(const string &s)
{
  infos("%s...", s.c_str());
  for(unsigned int i = 0; i <= WIDGET_MAX; i++)
  {
    if(wtypes[i] == s)
    {
      infos("trouve (%d)", i);
      return (WidgetType) i;
    }
  }
  infos("Type de widget non reconnu: '%s'", s.c_str());
  return WIDGET_NULL;
}

std::string     VueGenerique::type_vers_desc(VueGenerique::WidgetType type)
{
  unsigned int id = (unsigned int) type;
  if(id > WIDGET_MAX)
  {
    erreur("invalid type conversion (%d).", id);
    return "nullptr-widget";
  }
  return wtypes[id];
}


struct FabriqueWidgetElmt
{
  FabriqueWidget *fab;
  std::string id;
};

static std::vector<FabriqueWidgetElmt> fabriques;

int VueGenerique::enregistre_widget(std::string id, FabriqueWidget *fabrique)
{
  FabriqueWidgetElmt fwe;
  fwe.id = id;
  fwe.fab = fabrique;
  fabriques.push_back(fwe);
  return 0;
}


class SepV: public VueGenerique
{
public:
  SepV()
  {

  }
  Gtk::VSeparator sep;
  Gtk::Widget *get_gtk_widget(){return &sep;}
};

class SepH: public VueGenerique
{
public:
  SepH()
  {
  }
  Gtk::HSeparator sep;
  Gtk::Widget *get_gtk_widget(){return &sep;}
};

VueGenerique *VueGenerique::fabrique(Node modele_donnees,
                                   Node modele_vue,
                                   Controleur *controler)
{
  VueGenerique *res = nullptr;

  if(modele_vue.is_nullptr())
  {
    erreur("Modele de vue vide.");
    return nullptr;
  }

  auto id_widget = modele_vue.schema()->name.get_id();
  infos("Fabique [%s]...", id_widget.c_str());
  WidgetType type = desc_vers_type(id_widget);

  switch(type)
  {

  case WIDGET_NULL:
  {
    bool trouve = false;
    for(auto &fab: fabriques)
    {
      if(fab.id == id_widget)
      {
        infos("Appelle fabrique speciale [%s]...", id_widget.c_str());
        res = fab.fab->fabrique(modele_donnees, modele_vue, controler);
        trouve = true;
        break;
      }
    }
    if(!trouve)
      erreur("Widget invalide : %s.", id_widget.c_str());
    break;
  }

  case WIDGET_AUTO:
  {
    trace_verbeuse("Fabrique vue automatique");

    XPath chemin = XPath(modele_vue.get_attribute_as_string("modele"));

    if(chemin.length() == 0)
    {
      erreur("widget auto: l'attribut 'modele' doit etre specifie.");
      return nullptr;
    }

    Node mod = modele_donnees.get_child(chemin);

    if(mod.is_nullptr())
    {
      erreur("Modele non trouve.", chemin.c_str());
      return nullptr;
    }

    res = new NodeView(mod);
    break;
  }

  case WIDGET_TABLE:
  {
    trace_verbeuse("Fabrique vue table");
    res = new VueTable(modele_donnees, modele_vue, controler);
    break;
  }

  case WIDGET_VUE_SPECIALE:
  {
    trace_verbeuse("Fabrique vue speciale");
    res = new CustomWidget(modele_donnees, modele_vue, controler);
    break;
  }

  case WIDGET_TRIG_LAYOUT:
  {
    trace_verbeuse("Fabrique trig layout");
    res = new TrigLayout(modele_donnees, modele_vue);
    break;
  }

  case WIDGET_LIST_LAYOUT:
  {
    trace_verbeuse("Fabrique list layout");
    res = new ListLayout(modele_donnees, modele_vue, controler);
    break;
  }

  case WIDGET_FIELD_LIST:
  {
    trace_verbeuse("Fabrique field list");
    res = new   VueListeChamps(modele_donnees, modele_vue, controler);
    break;
  }

  case WIDGET_VBOX:
    res = new VueLineaire(1, modele_donnees, modele_vue, controler);
    break;
  case WIDGET_HBOX:
    res = new VueLineaire(0, modele_donnees, modele_vue, controler);
    break;
  case WIDGET_NOTEBOOK:
    res = new NoteBookLayout(modele_donnees, modele_vue, controler);
    break;
  case WIDGET_BUTTON_BOX:
    res = new HButtonBox(modele_donnees, modele_vue, controler);
    break;

  case WIDGET_SEP_V:
    res = new SepV();
    break;
  case WIDGET_SEP_H:
    res = new SepH();
    break;
  case WIDGET_GRID_LAYOUT:
    res = new SubPlot(modele_donnees, modele_vue, controler);
    break;

  case WIDGET_CADRE:
  {
    res = new VueCadre(modele_donnees, modele_vue, controler);
    break;
  }

  case WIDGET_FIELD:

  case WIDGET_FIXED_STRING:
  case WIDGET_BUTTON:
  case WIDGET_BORDER_LAYOUT:

  case WIDGET_FIXED_LAYOUT:
  case WIDGET_PANNEAU:
  case WIDGET_IMAGE:
  case WIDGET_LABEL:
  default:
  {
    auto s = type_vers_desc(type);
    erreur("Type de widget non gere (%d / %s)", (int) type, s.c_str());
  }
  }
  if(res != nullptr)
  {
    res->modele_vue = modele_vue;
    res->modele_donnees = modele_donnees;
  }
  return res;
}


SubPlot::SubPlot(Node &data_model, Node &view_model, Controleur *controler)
{
  unsigned int n = view_model.get_children_count();
  for(auto i = 0u; i < n; i++)
  {
    Node ch = view_model.get_child_at(i);
    auto v = VueGenerique::fabrique(data_model, ch, controler);

    enfants.push_back(v);

    auto x = ch.get_attribute_as_int("x"),
         y = ch.get_attribute_as_int("y"),
         ncols = ch.get_attribute_as_int("ncols"),
         nrows = ch.get_attribute_as_int("nrows");

    grille.attach(*(v->get_gtk_widget()), x, y, ncols, nrows);
    grille.set_column_homogeneous(true);
    grille.set_row_homogeneous(true);
    grille.set_row_spacing(5);
    grille.set_column_spacing(5);

    //vs.push_back(v);
    //auto orient = ch.get_attribute_as_string("orientation");
    //if(orient == "left")
    //  hpane.add1(*(v->get_gtk_widget()));
    //else if(orient == "right")
    //  hpane.add2(*(v->get_gtk_widget()));
    //else if(orient == "bottom")
    //  vbox.pack_start(*(v->get_gtk_widget()), Gtk::PACK_SHRINK);
    //else
    //  erreur("Orientation invalide dans un triglayout ('%s')", orient.c_str());
  }
}

Gtk::Widget *SubPlot::get_gtk_widget()
{
  return &grille;
}

/*******************************************************************
 *******************************************************************
 *               TRIG LAYOUT                                       *
 *******************************************************************
 *******************************************************************/

TrigLayout::TrigLayout(Node &data_model, Node &view_model_)
{

  this->modele_donnees = data_model;
  this->modele_vue = view_model_;

  //GenericView *v1 = nullptr, *v2 = nullptr, *v3 = nullptr;

  string s = this->modele_vue.to_xml(0,true);
  infos("trig layout, modele de vue = \n%s", s.c_str());

  std::vector<VueGenerique *> vs;

  vbox.pack_start(hpane, Gtk::PACK_EXPAND_WIDGET);
  vbox.pack_start(vsep, Gtk::PACK_SHRINK);

  unsigned int n = modele_vue.get_children_count();
  for(auto i = 0u; i < n; i++)
  {
    Node ch = modele_vue.get_child_at(i);
    auto v = VueGenerique::fabrique(data_model, ch);
    enfants.push_back(v);
    vs.push_back(v);
    auto orient = ch.get_attribute_as_string("orientation");
    if(orient == "left")
      hpane.add1(*(v->get_gtk_widget()));
    else if(orient == "right")
      hpane.add2(*(v->get_gtk_widget()));
    else if(orient == "bottom")
      vbox.pack_start(*(v->get_gtk_widget()), Gtk::PACK_SHRINK);
    else
      erreur("Orientation invalide dans un triglayout ('%s')", orient.c_str());
  }
}

Gtk::Widget *TrigLayout::get_gtk_widget()
{
  return &vbox;
}

/*void set_sensitive(bool sensitive)
{
}*/

VueCadre::VueCadre(Node &data_model, Node &view_model, Controleur *controler)
{
  if(view_model.get_children_count() == 0)
  {
    avertissement("Cadre sans enfant.");
    return;
  }

  cadre.set_border_width(5);
  cadre.set_label(view_model.get_localized_name());

  VueGenerique *gv = VueGenerique::fabrique(data_model, view_model.get_child_at(0), controleur);
  if(gv != nullptr)
  {
    enfants.push_back(gv);
    cadre.add(*(gv->get_gtk_widget()));
  }
}

Gtk::Widget *VueCadre::get_gtk_widget()
{
  return &cadre;
}

VueCadre::~VueCadre()
{

}

/*******************************************************************
 *******************************************************************
 *               BOX LAYOUT                                        *
 *******************************************************************
 *******************************************************************/

VueLineaire::VueLineaire(int vertical,
                     Node &modele_donnees,
                     Node &modele_vue,
                     Controleur *controleur)
{
  this->vertical = vertical;

  if(vertical)
    box = &vbox;
  else
    box = &hbox;

  unsigned int n = modele_vue.get_children_count();

  enfants.resize(n);

  for(unsigned int i = 0u; i < n; i++)
    enfants[i] = nullptr;

  for(unsigned int i = 0u; i < n; i++)
  {
    auto child = modele_vue.get_child_at(i);
    VueGenerique *gv = VueGenerique::fabrique(modele_donnees, child, controleur);

    if(gv == nullptr)
    {
      auto s = child.to_xml();
      erreur("Construction boite : echec lors de la construction d'un enfant:\n%s", s.c_str());
      continue;
    }

    assert(!gv->modele_vue.is_nullptr());


    unsigned int y;
    if(vertical)
      y = child.get_attribute_as_int("y");
    else
      y = child.get_attribute_as_int("x");
    if(y >= n)
    {
      erreur("y > nombre d'elements dans la boite (y = %d, n = %d).", y, n);
      return;
    }
    enfants[y] = gv;
  }

  // Classement suivant y


  for(unsigned int i = 0u; i < n; i++)
  {
    //auto child = view_model.get_child_at(i);
    VueGenerique *gv = enfants[i];//GenericView::fabrique(data_model, child, controler);
    //elems.push_back(gv);

    if(gv == nullptr)
    {
      avertissement("Vue boite : element de position %d non specifie.", i);
      continue;
    }
    
    std::string pack = gv->modele_vue.get_attribute_as_string("pack");
    bool pos_fin = gv->modele_vue.get_attribute_as_boolean("pos-end");

    trace_verbeuse("disposition h/v : elem = %d, pack = %s, pos fin = %d",
        i, pack.c_str(), pos_fin);

    Gtk::PackOptions po =  pack == "shrink" ? Gtk::PACK_SHRINK : Gtk::PACK_EXPAND_WIDGET;

    if(pos_fin)
      box->pack_end(*(gv->get_gtk_widget()), po);
    else
      box->pack_start(*(gv->get_gtk_widget()), po);
    gv->get_gtk_widget()->show();
  }
  box->show_all_children(true);
}

VueLineaire::~VueLineaire()
{
  for(VueGenerique *gv: enfants)
  {
    if(gv == nullptr)
      continue;
    //trace_verbeuse("remove de box...");
    box->remove(*(gv->get_gtk_widget()));
    //trace_verbeuse("supression widget...");
    delete gv;
  }
  enfants.clear();
  //trace_verbeuse("fin destructeur");
}

Gtk::Widget *VueLineaire::get_gtk_widget()
{
  return box;
}


/*******************************************************************
 *******************************************************************
 *           NOTEBOOK LAYOUT                                       *
 *******************************************************************
 *******************************************************************/

NoteBookLayout::NoteBookLayout(Node &data_model, Node &view_model, Controleur *controler)
{
  unsigned int n = view_model.get_children_count();

  notebook.set_scrollable(true);
  notebook.popup_enable();

  for(unsigned int i = 0; i < n; i++)
  {
    Node child = view_model.get_child_at(i);
    VueGenerique *gv = VueGenerique::fabrique(data_model, child, controler);
    enfants.push_back(gv);

    auto nom = child.get_localized_name();

    // TODO: in elems to be able to delete
    Gtk::HBox *ybox = new Gtk::HBox();
    Gtk::Label *lab = new Gtk::Label();
    lab->set_markup("<b> " + nom + "</b>");

    ybox->pack_start(*lab);
    notebook.append_page(*(gv->get_gtk_widget()), *ybox);
    ybox->show_all_children();
  }
  notebook.show_all_children(true);
}

NoteBookLayout::~NoteBookLayout()
{
  int n = notebook.get_n_pages();
  for (int i = 0; i < n; i++)
    notebook.remove_page(0);

  for(VueGenerique *gv: enfants)
  {
    //notebook.remove(*(gv->get_gtk_widget()));
    delete gv;
  }
  enfants.clear();
}

Gtk::Widget *NoteBookLayout::get_gtk_widget()
{
  return &notebook;
}


/*******************************************************************
 *******************************************************************
 *               HBBOX                                             *
 *******************************************************************
 *******************************************************************/

HButtonBox::HButtonBox(Node &data_model, Node &view_model_, Controleur *controler)
{
  this->data_model = data_model;
  this->controler  = controler;
  this->view_model = view_model_;//.get_child("button-box");

  unsigned int n = view_model.get_children_count("bouton");
  actions.resize(n);
  for(unsigned int i = 0; i < n; i++)
  {
    Node bouton = view_model.get_child_at("bouton", i);

    actions[i].name = bouton.name();
    actions[i].button = new Gtk::Button();
    std::string s = bouton.get_localized_name();
    actions[i].button->set_label(s);
    hbox.pack_start(*(actions[i].button), Gtk::PACK_SHRINK);
    actions[i].button->set_border_width(4);

    actions[i].button->signal_clicked().connect(
        sigc::bind<std::string> (
        sigc::mem_fun(*this, &HButtonBox::on_button), actions[i].name));

  }
  hbox.set_layout(Gtk::BUTTONBOX_END);
}

Gtk::Widget *HButtonBox::get_gtk_widget()
{
  return &hbox;
}

void HButtonBox::on_button(std::string action)
{
  trace_verbeuse("Action requise: [%s]", action.c_str());
  if(controler == nullptr)
  {
    avertissement("Pas de controleur.");
  }
  else
    controler->gere_action(action, data_model);
}

/*******************************************************************
 *******************************************************************
 * CUSTOM WIDGET                                                   *
 *******************************************************************
 *******************************************************************/

CustomWidget::CustomWidget(Node &modele_donnees,
                           Node &modele_vue,
                           Controleur *controleur)
{
  this->controler = controleur;
  this->id = modele_vue.get_attribute_as_string("name");
  widget = nullptr;
}

void CustomWidget::maj_contenu_dynamique()
{
  infos("  ++++ CustomWidget::maj_contenu_dynamique ++++");
  if(widget != nullptr)
  {
    evt_box.remove();
    delete widget;
    widget = nullptr;
  }
  controler->genere_contenu_dynamique(id, data_model, &widget);
  if(widget == nullptr)
  {
    auto l = new Gtk::Label();
    widget = l;
    char bf[500];
    sprintf(bf, "Widget à faire : %s", id.c_str());
    l->set_label(bf);
  }
  evt_box.add(*widget);
}

Gtk::Widget *CustomWidget::get_gtk_widget()
{
  maj_contenu_dynamique();
  return &evt_box;
  /*Gtk::Widget *res = nullptr;
  controler->genere_contenu_dynamique(id, data_model, &res);
  if(res == nullptr)
  {
    auto l = new Gtk::Label();
    char bf[500];
    sprintf(bf, "Widget à faire : %s", id.c_str());
    l->set_label(bf);
    return l;
  }
  return res;*/
}

/*******************************************************************
 *******************************************************************
 *               LIST LAYOUT                                       *
 *******************************************************************
 *******************************************************************/

void ListLayout::rebuild_view()
{
  for(Elem &e: elems)
  {
    table.remove(*(e.frame));
    delete e.frame;
    //e.frame->remove();

    delete e.label;

    if(e.widget != nullptr)
      delete e.widget;
  }
  //enfants.clear();
  elems.clear();
  std::string tp = modele_vue.get_attribute_as_string("child-type");

  XPath ptp(tp);

  XPath prefix = ptp.remove_last();

  std::string postfix = ptp.get_last();
  Node rnode = modele_donnees;
  if(prefix.length() > 0)
    rnode = rnode.get_child(prefix);



  unsigned int i = 0, n = rnode.get_children_count(postfix);

  /*trace_major("tp = %s, prefix = %s, n = %d.", tp.c_str(), prefix.c_str(), n);
  auto ss = rnode.to_xml();
  trace_verbeuse("rnode = %s.\n", ss.c_str());
  ss = data_model.to_xml();
  trace_verbeuse("data_model = %s.\n", ss.c_str());*/

  unsigned int ncols = modele_vue.get_attribute_as_int("ncols");
  uint16_t x = 0, y = 0;
  uint16_t ny = (n + ncols - 1) / ncols;
  table.resize(ncols, ny);
  elems.resize(n);
  for(i = 0; i < n; i++)
  {
    elems[i].id = i;
    elems[i].label = new SensitiveLabel(utils::str::int2str(i));
    elems[i].label->add_listener(this, &ListLayout::on_click);


    elems[i].widget = nullptr;
    elems[i].model = rnode.get_child_at(postfix, i);
    controler->genere_contenu_dynamique(modele_vue.get_attribute_as_string("child-type"),
                               elems[i].model, &elems[i].widget);

    //if(elems[i].widget != nullptr)
      //enfants.push_back(elems[i].widget);

    elems[i].vbox = new Gtk::VBox();
    elems[i].frame = new Gtk::Frame();
    elems[i].align = new Gtk::Alignment(0.5,0.5,0,0);
    elems[i].align2 = new Gtk::Alignment(0.5,0.5,0,0);

    elems[i].align2->set_padding(3,3,3,3);

    //elems[i].align2->set_padding(10,10,10,10);
    elems[i].evt_box = new Gtk::EventBox();
    elems[i].evt_box->add(*(elems[i].align));

    elems[i].evt_box->set_events(Gdk::BUTTON_PRESS_MASK /*| Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_RELEASE_MASK*/);
    elems[i].evt_box->signal_button_press_event().connect(sigc::bind<Elem *>(sigc::mem_fun(this,
        &ListLayout::on_button_press_event), &(elems[i])));;

    table.attach(*(elems[i].evt_box), x, x+1, y, y+1);//, Gtk::SHRINK, Gtk::SHRINK);
    elems[i].align->add(*(elems[i].frame));
    elems[i].align2->add(*(elems[i].vbox));
    elems[i].vbox->pack_start(*(elems[i].label), Gtk::PACK_SHRINK);

    if(elems[i].widget != nullptr)
      elems[i].vbox->pack_start(*(elems[i].widget), Gtk::PACK_SHRINK);

    //elems[i].label->set_border_width(15); // test, to remove
    elems[i].align2->set_border_width(5);//15);
    elems[i].frame->add(*(elems[i].align2));
    if(x == ncols - 1)
      y++;
    x = (x + 1) % ncols;
  }
  table.show_all_children(true);

  if(current_selection >= (int) elems.size())
    current_selection = -1;

  update_view();
}

bool ListLayout::on_button_press_event(GdkEventButton *evt, Elem *elt)
{
  trace_verbeuse("Bpress event sur element list layout (type = %d).", (int) evt->type);

  current_selection = elt->id;
  update_view();

  if((evt->type == GDK_2BUTTON_PRESS) && (evt->button == 1))
  {
    trace_verbeuse("Double click.");
    for(Action &a: actions)
    {
      if(a.is_default)
      {
        controler->gere_action(a.name,
                             get_selection());
        return 0;
      }
    }
  }
  else if((evt->type == GDK_BUTTON_PRESS) && (evt->button == 1))
  {
    trace_verbeuse("simple clock");
  }

  return true;
}

void ListLayout::on_click(const LabelClick &click)
{
  trace_verbeuse("click detected: %s, type = %s", click.path.c_str(), (click.type == LabelClick::VAL_CLICK) ? "val" : "sel");
  int id = atoi(click.path.c_str());
  current_selection = id;
  update_view();

  if(click.type == LabelClick::VAL_CLICK)
  {
    trace_verbeuse("Double click.");
    for(Action &a: actions)
    {
      if(a.is_default)
      {
        controler->gere_action(a.name,
                             get_selection());
        return;
      }
    }
  }
}

/*#ifdef LINUX

# define BCOL_INACTIVE "#000080"
# define BCOL_ACTIVE   "#FF00FF"

#else

# define BCOL_INACTIVE "#202020"
# define BCOL_ACTIVE   "#400040"

#endif*/

# define BCOL_INACTIVE "#000080"
# define BCOL_ACTIVE   "#FF00FF"

#if 0
# define BCOL_INACTIVE "#202020"
# define BCOL_ACTIVE   "#400040"
#endif

#define OVERRIDE_COLORS

void ListLayout::update_view()
{
  for(auto i = 0u; i < elems.size(); i++)
  {
    //elems[i].frame->set_border_width(0);
#   ifdef OVERRIDE_COLORS
    elems[i].frame->set_border_width(0);
    elems[i].frame->set_shadow_type(Gtk::SHADOW_NONE);
    elems[i].frame->override_color(Gdk::RGBA(BCOL_INACTIVE), Gtk::STATE_FLAG_NORMAL);
#   endif

    //elems[i].event_box->override_color(Gdk::RGBA(BCOL_INACTIVE),
    //                                   Gtk::STATE_FLAG_NORMAL);

    Gtk::Widget *unused;
    elems[i].label->label.set_markup(controler->genere_contenu_dynamique(
        modele_vue.get_attribute_as_string("child-type"),
                                     elems[i].model, &unused));
  }

  if(current_selection != -1)
  {
    /*assert(current_selection < (int) elems.size());
    elems[current_selection].frame->set_border_width(5);*/
#   ifdef OVERRIDE_COLORS
    elems[current_selection].frame->set_border_width(5);
    elems[current_selection].frame->set_shadow_type(Gtk::SHADOW_OUT);
    elems[current_selection].frame->override_color(Gdk::RGBA(BCOL_ACTIVE), Gtk::STATE_FLAG_NORMAL);
#   endif

    Gtk::Widget *unused;
    auto s = controler->genere_contenu_dynamique(this->modele_vue.get_attribute_as_string("child-type"),
					elems[current_selection].model, &unused);
    //s = "<big>"+s+"</big>";
    //elems[current_selection].label->label.set_markup(s);

    //elems[current_selection].event_box->modify_bg(Gtk::STATE_FLAG_NORMAL, Gdk::RGBA(BCOL_ACTIVE));
    //elems[current_selection].event_box->override_color(Gdk::RGBA(BCOL_ACTIVE), Gtk::STATE_FLAG_NORMAL);

    //
  }

  for(Action &a: actions)
  {
    if((current_selection == -1) && (a.need_sel))
      a.button->set_sensitive(false);
    else
      a.button->set_sensitive(true);
  }
}

void ListLayout::on_event(const ChangeEvent &ce)
{
  auto s = ce.to_string();
  switch(ce.type)
  {
  case ChangeEvent::CHILD_ADDED:
  case ChangeEvent::CHILD_REMOVED:
    //if(data_model_.get_child())
    if(ce.path.length() <= 2) // Ne regarde que des changements qui le concerne directement
    {
      trace_verbeuse("Changement sur list layout: %s.", s.c_str());
      rebuild_view();
    }
    break;
  case ChangeEvent::GROUP_CHANGE:
    //update_view();
    break;
  case ChangeEvent::ATTRIBUTE_CHANGED:
  case ChangeEvent::COMMAND_EXECUTED:
    break;
  }
}

Node ListLayout::get_selection()
{
  Node res;
  if((current_selection == -1) || (current_selection >= (int) elems.size()))
    return modele_donnees;
  return elems[current_selection].model;
}

void ListLayout::on_button(std::string action)
{
  trace_verbeuse("action detected: %s", action.c_str());
  controler->gere_action(action, get_selection());
}

void ListLayout::maj_contenu_dynamique()
{
  rebuild_view();
}


ListLayout::ListLayout(Node &data_model, Node &view_model_, Controleur *controler)
{
  current_selection = -1;
  this->controler = controler;
  this->modele_donnees = data_model;
  this->modele_vue = view_model_;

  unsigned int n = modele_vue.get_children_count("bouton");
  actions.resize(n);
  for(unsigned int i = 0; i < n; i++)
  {
    Node bouton = modele_vue.get_child_at("bouton", i);

    actions[i].need_sel   = bouton.get_attribute_as_boolean("require-sel");
    actions[i].is_default = bouton.get_attribute_as_boolean("default");
    actions[i].name = bouton.name();
    actions[i].button = new Gtk::Button();
    std::string s = bouton.get_localized_name();
    actions[i].button->set_label(s);
    hbox.pack_start(*(actions[i].button), Gtk::PACK_SHRINK);
    actions[i].button->set_border_width(4);
    actions[i].button->signal_clicked().connect(
        sigc::bind<std::string> (
        sigc::mem_fun(*this, &ListLayout::on_button), actions[i].name));

  }
  hbox.set_layout(Gtk::BUTTONBOX_END);

  vbox.pack_start(table, Gtk::PACK_EXPAND_WIDGET);


  rebuild_view();

  scroll.add(vbox);
  scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  scroll.set_min_content_width(500);
  scroll.set_min_content_height(400);

  /*vbox*/scroll.show_all_children(true);

  vbox_ext.pack_start(scroll, Gtk::PACK_EXPAND_WIDGET);
  vbox_ext.pack_start(vsep, Gtk::PACK_SHRINK);
  vbox_ext.pack_start(hbox, Gtk::PACK_SHRINK);

  data_model.add_listener(this);
}

ListLayout::~ListLayout()
{
  modele_donnees.remove_listener(this);
}

Gtk::Widget *ListLayout::get_gtk_widget()
{
  return &vbox_ext;
}


/*******************************************************************
 *******************************************************************
 *               FIELD LIST VIEW                                   *
 *******************************************************************
 *******************************************************************/

void VueListeChamps::set_sensitive(bool sensitive)
{
  VueGenerique::set_sensitive(sensitive);
  for(auto &f: champs)
  {
    if(f->av != nullptr)
    {
      f->av->set_sensitive(sensitive);
      f->label.set_sensitive(sensitive);
      f->label_unit.set_sensitive(sensitive);
    }
  }
}

VueListeChamps::VueListeChamps(Node &modele_donnees,
                               Node &modele_vue,
                               Controleur *controleur_)
{
  this->modele_vue = modele_donnees;
  this->modele_donnees = modele_vue;
  this->controleur = controleur_;

  unsigned int i, n = modele_vue.get_children_count("champs");

  trace_verbeuse("Field list view: n rows = %d.", n);
  table.resize(n, 3);

  for(i = 0; i < n; i++)
  {
    Node modele_vue_champs = modele_vue.get_child_at("champs", i);
    XPath chemin = XPath(modele_vue_champs.get_attribute_as_string("modele"));
    std::string ctrl_id = modele_vue_champs.get_attribute_as_string("ctrl-id");
    //bool editable = field.get_attribute_as_boolean("editable");

    if(chemin.length() == 0)
    {
      erreur("%s: field-view: 'model' attribute not specified.");
      continue;
    }

    Node att_owner = modele_donnees.get_child(chemin.remove_last());
    string att_name = chemin.get_last();

    if(att_owner.is_nullptr())
    {
      erreur("Field '%s': model not found.", chemin.c_str());
      continue;
    }

    if(!att_owner.schema()->has_attribute(att_name))
    {
      auto s = att_owner.to_xml();
      erreur("Field '%s': no such attribute in the model.\nModel = \n%s",
                  chemin.c_str(), s.c_str());
      continue;
    }

    auto att_schema = att_owner.schema()->get_attribute(att_name);

    ChampsCtx *fc = new ChampsCtx();
    champs.push_back(fc);

    fc->av = AttributeView::factory(modele_donnees, modele_vue_champs);

    if(fc->av != nullptr)
      fc->av->CProvider<KeyPosChangeEvent>::add_listener(this);

    fc->label.set_use_markup(true);

    Localized nom = att_schema->name;

    if(modele_vue_champs.get_localized_name().size() > 0)
      nom = modele_vue_champs.get_localized();

    std::string s = NodeView::mk_label_colon(nom);

    if(appli_view_prm.overwrite_system_label_color)
      s = "<span foreground='" + appli_view_prm.att_label_color.to_html_string() + "'>" + s + "</span>";

    s = "<b>" + s + "</b>";

    fc->label.set_markup(s);

    fc->align[0].set(Gtk::ALIGN_START, Gtk::ALIGN_CENTER, 0, 0);
    fc->align[1].set(Gtk::ALIGN_START, Gtk::ALIGN_CENTER, 1, 0);
    fc->align[2].set(Gtk::ALIGN_START, Gtk::ALIGN_CENTER, 1, 0);
    
    fc->align[0].add(fc->label);

    Gtk::Widget *widget = nullptr;

    if(fc->av != nullptr)
      widget = fc->av->get_gtk_widget();

    if(widget != nullptr)
      fc->align[1].add(*widget); 

    table.attach(fc->align[0], 0, 1, i, i + 1, Gtk::FILL, Gtk::FILL, 5, 5);
    table.attach(fc->align[1], 1, 2, i, i + 1, Gtk::FILL, Gtk::FILL, 5, 5);
    
    if(att_schema->has_unit())
    {
      fc->align[2].add(fc->label_unit);
      fc->label_unit.set_label(att_schema->unit);
      table.attach(fc->align[2], 2, 3, i, i + 1, Gtk::FILL, Gtk::FILL, 5, 5);
    }

    if(att_schema->has_description()) 
    {
      fc->label.set_has_tooltip();
      fc->label.set_tooltip_markup(mk_html_tooltip(att_schema));
    }
  }
}

VueListeChamps::~VueListeChamps()
{
  for(unsigned int i = 0; i < champs.size(); i++)
  {
    delete champs[i];
    champs[i] = nullptr;
  }
}

Gtk::Widget *VueListeChamps::get_gtk_widget()
{
  return &table;
}

void VueListeChamps::on_event(const ChangeEvent &ce)
{
}

void VueListeChamps::update_langue()
{
}

}
}

