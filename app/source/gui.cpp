#include <iostream>
#include "utils.h"
#include "engine.h"
#include "project.h"
#include <filesystem>
#include <gtkmm.h>
#include "lilvutils.h"

namespace 
{

void DoAndShowException(std::function<void()> f)
{
    try
    {
        f();
    }
    catch(const std::exception &e)
    {
        std::string msg = e.what();
        // show in gtk messafe box:
        Gtk::MessageDialog dialog(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        dialog.run();
    }
}

class TEditPortsPanel : public Gtk::Frame
{
public:
    class TRow
    {
    public:
        Gtk::Entry m_Entry;
        Gtk::Button m_RemoveButton {"Remove"};
    };
    TEditPortsPanel(std::vector<std::string> && portNames, jackutils::PortKind kind, jackutils::PortDirection direction) : m_PortNames(std::move(portNames)), m_Kind(kind), m_Direction(direction)
    {
        auto portnames = jackutils::Client::Static().GetAllPorts(kind, direction);
        add(m_ScrolledWindow);
        m_ScrolledWindow.set_border_width(5);
        m_ScrolledWindow.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        m_ScrolledWindow.add(m_Box);
        m_Box.pack_start(m_Grid, Gtk::PACK_EXPAND_WIDGET);
        m_ScrolledWindow.set_size_request(500,130);
        m_Box.pack_start(m_AddButton, Gtk::PACK_SHRINK);
        m_AddButton.set_halign(Gtk::ALIGN_START);
        for(const auto &portname: portnames)
        {
            m_MenuItems.emplace_back(portname);
            m_MenuItems.back().signal_activate().connect([this, portname](){
                m_PortNames.push_back(portname);
                m_OnChange.emit();
                data2grid();
            });
            m_PopupMenu.add(m_MenuItems.back());
        }
        if(!portnames.empty())
        {
            m_PopupMenu.add(m_MenuSeparator);
        }
        m_OtherMenuItem.signal_activate().connect([this](){
            m_PortNames.push_back("");
            m_OnChange.emit();
            data2grid();
        });
        m_PopupMenu.add(m_OtherMenuItem);
        m_PopupMenu.show_all();
        m_AddButton.set_popup(m_PopupMenu);
        m_AddButton.set_label("Add port");
        show_all_children(true);
        data2grid();
    }
    ~TEditPortsPanel()
    {
        m_Destructing = true;
    }
    const std::vector<std::string>& PortNames() const {return m_PortNames;}
    void data2grid()
    {
        while(m_Rows.size() < m_PortNames.size())
        {
            auto index = m_Rows.size();
            m_Rows.push_back(TRow());
            auto &row = m_Rows.back();
            m_Grid.attach(row.m_Entry, 0, index);
            m_Grid.attach(row.m_RemoveButton, 1, index);
            row.m_Entry.show();
            row.m_RemoveButton.show();
            row.m_RemoveButton.set_hexpand(false);
            row.m_RemoveButton.set_halign(Gtk::ALIGN_START);
            row.m_RemoveButton.signal_clicked().connect([this, index]() {
                if (index < m_PortNames.size()) {
                    m_PortNames.erase(m_PortNames.begin() + index);
                    m_OnChange.emit();
                    data2grid();
                }
            });
            row.m_Entry.set_hexpand(true);
            row.m_Entry.signal_changed().connect([this, index]() {
                if (index < m_PortNames.size()) {
                    auto txt = utils::trim(m_Rows.at(index).m_Entry.get_text());
                    if(m_PortNames.at(index) != txt)
                    {
                        m_PortNames.at(index) = std::move(txt);
                        m_OnChange.emit();
                    }
                }
            });
            row.m_Entry.signal_focus_out_event().connect([this](GdkEventFocus* event) {
                if(!m_Destructing)
                {
                    data2grid();
                }
                return false;  // Continue with the default handler
            });
        }
        if(m_Rows.size() > m_PortNames.size())
        {   
            m_Rows.resize(m_PortNames.size());
        }
        for(size_t i = 0; i < m_PortNames.size(); i++)
        {
            m_Rows.at(i).m_Entry.set_text(utils::trim(m_PortNames.at(i)));
        }

    }
    auto& signal_value_changed() {return m_OnChange;}

private:
    std::vector<std::string> m_PortNames;
    std::vector<TRow> m_Rows;
    jackutils::PortKind m_Kind;
    jackutils::PortDirection m_Direction;
    Gtk::MenuButton m_AddButton;
    Gtk::Menu m_PopupMenu;
    std::vector<Gtk::MenuItem> m_MenuItems;
    Gtk::SeparatorMenuItem m_MenuSeparator;
    Gtk::MenuItem m_OtherMenuItem {"Other..."};
    Gtk::Grid m_Grid;
    Gtk::Box m_Box {Gtk::ORIENTATION_VERTICAL};
    bool m_Destructing = false;
    sigc::signal<void> m_OnChange;
    Gtk::ScrolledWindow m_ScrolledWindow;
};

class TEditSettingsDialog : public Gtk::Dialog
{
public:
    TEditSettingsDialog(project::TJackConnections &&conn) : m_JackConnections(std::move(conn)), m_AudioOutPortPanels({TEditPortsPanel(std::vector<std::string>{m_JackConnections.AudioOutputs()[0]}, jackutils::PortKind::Audio, jackutils::PortDirection::Input), TEditPortsPanel(std::vector<std::string>{m_JackConnections.AudioOutputs()[1]}, jackutils::PortKind::Audio, jackutils::PortDirection::Input)}), m_VocoderInputPanel(std::vector<std::string>{m_JackConnections.VocoderInput()}, jackutils::PortKind::Audio, jackutils::PortDirection::Output)
    {
        set_title("Settings");
        set_modal(true);
        size_t row = 0;
        m_Grid.set_row_spacing(5);
        get_content_area()->add(m_Grid);
        m_Grid.attach(m_BufferSizeLabel, 0, row);
        m_Grid.attach(m_BufferSizeEntry, 1, row++);
        for(size_t i = 0; i < 2; i++)
        {
            m_Grid.attach(m_AudioOutPortLabels[i], 0, row);
            m_Grid.attach(m_AudioOutPortPanels[i], 1, row++);
            m_AudioOutPortPanels[i].signal_value_changed().connect([this, i](){
                auto outports = m_JackConnections.AudioOutputs();
                outports[i] = m_AudioOutPortPanels[i].PortNames();
                m_JackConnections = m_JackConnections.ChangeAudioOutputs(std::move(outports));
            });
        }
        m_Grid.attach(m_VocoderInputLabel, 0, row);
        m_Grid.attach(m_VocoderInputPanel, 1, row++);
        m_VocoderInputPanel.signal_value_changed().connect([this](){
            auto vocoderinput = m_VocoderInputPanel.PortNames();
            m_JackConnections = m_JackConnections.ChangeVocoderInput(std::move(vocoderinput));
        });
        for(auto label: {&m_BufferSizeLabel, &m_VocoderInputLabel, &m_AudioOutPortLabels[0], &m_AudioOutPortLabels[1]})
        {
            label->set_halign(Gtk::ALIGN_START);
        }
        m_BufferSizeEntry.set_size_request(50,-1);
        m_BufferSizeEntry.set_halign(Gtk::ALIGN_START);
        m_BufferSizeEntry.signal_changed().connect([this](){
            try
            {
                auto v = (int)utils::to_int64(m_BufferSizeEntry.get_text());
                if(v != m_JackConnections.BufferSize())
                {
                    m_JackConnections = m_JackConnections.ChangeBufferSize(v);
                }
            }
            catch(...) {}
        });
        m_BufferSizeEntry.signal_focus_out_event().connect([this](GdkEventFocus* event) {
            if(!m_Destructing)
            {
                data2dlg();
            }
            return false;  // Continue with the default handler
        });
        add_action_widget(m_CancelButton, Gtk::RESPONSE_CANCEL);
        add_action_widget(m_OkButton, Gtk::RESPONSE_OK);
        data2dlg();
        show_all_children();
    }
    const project::TJackConnections& JackConnections() const { return m_JackConnections; }
    ~TEditSettingsDialog()
    {
        m_Destructing = true;
    }
    void data2dlg()
    {
        m_BufferSizeEntry.set_text(std::to_string(m_JackConnections.BufferSize()));
    }
private:
    project::TJackConnections m_JackConnections;
    Gtk::Grid m_Grid;
    Gtk::Label m_BufferSizeLabel {"Audio buffer size:"};
    Gtk::Entry m_BufferSizeEntry;
    std::array<Gtk::Label, 2> m_AudioOutPortLabels {Gtk::Label("Left audio output:"), Gtk::Label("Right audio output:")};
    std::array<TEditPortsPanel, 2> m_AudioOutPortPanels;
    Gtk::Label m_VocoderInputLabel {"Vocoder input:"};
    TEditPortsPanel m_VocoderInputPanel;
    Gtk::Button m_OkButton {"OK"};
    Gtk::Button m_CancelButton {"Cancel"};
    bool m_Destructing = false;
};

class TLevelSlider : public Gtk::Box
{
public:
    TLevelSlider(const TLevelSlider &other) = delete;
    TLevelSlider()
    {
        set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        pack_start(m_Scale, Gtk::PACK_EXPAND_WIDGET);
        pack_start(m_Label, Gtk::PACK_SHRINK);
        m_Label.set_halign(Gtk::ALIGN_START);
        m_Scale.set_size_request(100, -1);
        m_Label.set_size_request(40, -1);
        m_Label.set_halign(Gtk::ALIGN_END);
        m_Scale.set_range(0.0, 1.0);
        m_Scale.set_draw_value(false);
        m_Scale.signal_value_changed().connect([this](){
            if(!m_NoRecursion)
            {
                m_NoRecursion = true;
                utils::finally finally([this](){m_NoRecursion = false;});
                auto multiplier = engine::sliderValueToMultiplier(m_Scale.get_value());
                if(multiplier != m_Multiplier)
                {
                    m_Multiplier = multiplier;
                    Update();
                    m_OnChange.emit();
                }
            }
        });
        show_all_children(false);
        Update();
    }
    double Multiplier() const { return m_Multiplier; }
    void SetMultiplier(double v)
    {
        if(m_Multiplier == v) return;
        m_Multiplier = v;
        Update();
    }
    auto& signal_value_changed() {return m_OnChange;}

private:
    void Update()
    {
        if(!m_NoRecursion)
        {
            m_NoRecursion = true;
            utils::finally finally([this](){m_NoRecursion = false;});
            m_Scale.set_value(engine::multiplierToSliderValue(m_Multiplier));
        }
        m_Label.set_label(engine::multiplierToText(m_Multiplier));
    }

private:
    Gtk::Scale m_Scale;
    Gtk::Label m_Label;
    bool m_NoRecursion = false;
    double m_Multiplier = 1;
    sigc::signal<void> m_OnChange;
};

class PluginSelectorDialog : public Gtk::Dialog
{
public:
    PluginSelectorDialog(PluginSelectorDialog &&other) = delete;
    PluginSelectorDialog(const PluginSelectorDialog &other) = delete;
    PluginSelectorDialog(const std::string &selectedplugin)
    {
        set_title("Select Plugin");
        set_default_size(800, 400);
        add_button("Cancel", Gtk::RESPONSE_CANCEL);
        m_OkButton = add_button("OK", Gtk::RESPONSE_OK);
        m_PluginListScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        m_PluginListScrolledWindow.add(m_PluginList);
        get_vbox()->pack_start(m_PluginListScrolledWindow, Gtk::PACK_EXPAND_WIDGET);

        m_PluginList.signal_cursor_changed().connect([this](){
            Gtk::TreeModel::Path path;
            Gtk::TreeViewColumn* focus_column;
            m_PluginList.get_cursor(path, focus_column);
            std::optional<std::string> selectedpluginuri;
            if(path)
            {
                auto iter = m_ListStore->get_iter(path);
                if(iter)
                {
                    selectedpluginuri = iter->get_value(m_Columns.m_Lv2Uri);
                }
            }
            m_SelectedPluginUri = selectedpluginuri;
            enableItems();
        });
        m_PluginList.set_model(m_ListStore);
        m_PluginList.append_column("Name", m_Columns.m_Name);
        m_PluginList.append_column("Class", m_Columns.m_Class);
        Populate();
        if(!selectedplugin.empty())
        {
            for(auto iter = m_ListStore->children().begin(); iter != m_ListStore->children().end(); iter++)
            {
                const auto &row = *iter;
                if((std::string)row[m_Columns.m_Lv2Uri] == selectedplugin)
                {
                    m_PluginList.get_selection()->select(iter);
                    m_PluginList.scroll_to_row(m_ListStore->get_path(iter), 0.5);
                    break;
                }
            }
        }
        enableItems();
        show_all();
    }
    void enableItems()
    {
        bool canok = true;
        if(!m_SelectedPluginUri)
        {
            canok = false;
        }
        m_OkButton->set_sensitive(canok);
    }
    void Populate()
    {
        const auto &pluginlist = lilvutils::World::Static().PluginList();
        auto plugins = pluginlist.Plugins();
        std::sort(plugins.begin(), plugins.end(), [](const auto &a, const auto &b){
            // case insensitive:
            std::string aName = a.Name();
            std::string bName = b.Name();
            std::transform(aName.begin(), aName.end(), aName.begin(), [](unsigned char c){ return std::tolower(c); });
            std::transform(bName.begin(), bName.end(), bName.begin(), [](unsigned char c){ return std::tolower(c); });
            return aName < bName;
        });
        for(const auto &plugin: plugins)
        {
            Gtk::TreeModel::Row row = *(m_ListStore->append());
            row[m_Columns.m_Name] = plugin.Name();
            row[m_Columns.m_Lv2Uri] = plugin.Uri();
            row[m_Columns.m_Class] = pluginlist.Classes().at(plugin.ClassIndex()).Description();
        }
    }
    std::optional<std::string> SelectedPluginUri() const { return m_SelectedPluginUri; }

private:
    class Columns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Columns()
        {
            add(m_Name);
            add(m_Class);
            add(m_Lv2Uri);
        }
        Gtk::TreeModelColumn<std::string> m_Name;
        Gtk::TreeModelColumn<std::string> m_Class;
        Gtk::TreeModelColumn<std::string> m_Lv2Uri;
    };
    Gtk::ScrolledWindow m_PluginListScrolledWindow;
    std::optional<std::string> m_SelectedPluginUri;
    Columns m_Columns;
    Glib::RefPtr<Gtk::ListStore> m_ListStore {Gtk::ListStore::create(m_Columns)};
    Gtk::TreeView m_PluginList;
    Gtk::Button *m_OkButton = nullptr;
};

class TEditParametersPanel : public Gtk::Frame {
public:
    TEditParametersPanel(TEditParametersPanel &&) = delete;
    TEditParametersPanel(const TEditParametersPanel &) = delete;
    
    TEditParametersPanel(std::vector<project::TInstrument::TParameter> &&params)
        : m_Parameters(std::move(params)), m_AddButton("Add controller")
    {
        m_AddButton.signal_clicked().connect([this]() {
            m_Parameters.emplace_back(1, std::optional<int>(), "New controller");
            m_OnChange.emit();
            Update();  // Sync grid with the updated parameters
        });
        add(m_ScrolledWindow);
        m_ScrolledWindow.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        m_ScrolledWindow.add(m_Box);
        m_ScrolledWindow.set_size_request(400, 140);
        m_ScrolledWindow.set_border_width(5);

        // Add grid and button to the panel
        m_Box.pack_start(m_Grid, Gtk::PACK_EXPAND_WIDGET);
        m_Box.pack_start(m_AddButton, Gtk::PACK_SHRINK);
        m_AddButton.set_halign(Gtk::ALIGN_START);
        m_Grid.set_hexpand(true);
        m_Grid.set_vexpand(true);
        for(size_t i = 0; i < 3; i++)
        {
            m_Grid.attach(m_RowHeadersLabel.at(i), i, 0, 1, 1);
        }
 
        Update();  // Initialize the grid with the initial data
    }

    sigc::signal<void>& onchange() {
        return m_OnChange;
    }

    const std::vector<project::TInstrument::TParameter>& Parameters() const {
        return m_Parameters;
    }
    void Update() {
        // Resize m_Rows to match the size of m_Parameters
        while (m_Rows.size() < m_Parameters.size()) {
            auto index = m_Rows.size();
            auto row = std::make_unique<TRow>(index, this);
            size_t colindex = 0;
            for(auto control: {(Gtk::Widget *)&row->m_ControllerEntry, (Gtk::Widget *)&row->m_ValueEntry, (Gtk::Widget *)&row->m_LabelEntry, (Gtk::Widget *)&row->m_RemoveButton})
            {
                m_Grid.attach(*control, colindex++, index+1, 1, 1);
                control->set_visible(true);
            }
            m_Rows.push_back(std::move(row));
        }
        if (m_Rows.size() > m_Parameters.size()) {
            m_Rows.resize(m_Parameters.size());
        }


        // Update each row's entries to reflect the current state of m_Parameters
        for (size_t i = 0; i < m_Parameters.size(); ++i) {
            m_Rows.at(i)->update();
        }

        for(size_t i = 0; i < 3; i++)
        {
            m_RowHeadersLabel.at(i).set_visible(!m_Parameters.empty());
        }
    }

private:
    class TRow {
    public:
        TRow(size_t index, TEditParametersPanel* parent)
            : m_Index(index),
              m_RemoveButton("Remove"),
              m_Parent(parent) 
        {
            m_ControllerEntry.signal_changed().connect([this]() {
                try
                {
                    auto v = utils::to_int64(m_ControllerEntry.get_text());
                    if( (v >= 1) && (v < 120) )
                    {
                        auto &param = m_Parent->m_Parameters.at(m_Index);
                        if(param.ControllerNumber() != v)
                        {
                            param = param.ChangeControllerNumber((int)v);
                            m_Parent->m_OnChange.emit();
                        }
                    }
                }
                catch(...) {}
            });

            m_ValueEntry.signal_changed().connect([this]() {
                try
                {
                    auto v = utils::to_optional_int64(m_ValueEntry.get_text());
                    if( (!v) || ((v.value() >= 0) && (v.value() <= 127)) )
                    {
                        auto &param = m_Parent->m_Parameters.at(m_Index);
                        if(param.InitialValue() != std::optional<int>(v))
                        {
                            param = param.ChangeInitialValue(std::optional<int>(v));
                            m_Parent->m_OnChange.emit();
                        }
                    }
                }
                catch(...) {}
            });

            m_LabelEntry.signal_changed().connect([this]() {
                auto v = m_LabelEntry.get_text();
                v = utils::trim(v);
                auto &param = m_Parent->m_Parameters.at(m_Index);
                if(param.Label() != v)
                {
                    param = param.ChangeLabel(std::move(v));
                    m_Parent->m_OnChange.emit();
                }
            });

            m_RemoveButton.signal_clicked().connect([this]() {
                m_Parent->on_remove_button_clicked(m_Index);
            });

            for(auto entry: {&m_ControllerEntry, &m_ValueEntry, &m_LabelEntry})
            {
                entry->signal_focus_out_event().connect([this](GdkEventFocus* event) {
                    this->update();
                    return false;  // Continue with the default handler
                });
            }
        }

        void update() {
            if(!m_Destroying) // we may get called from signal_focus_out_event() after destruction has begun
            {
                auto& param = m_Parent->m_Parameters.at(m_Index);
                m_ControllerEntry.set_text(std::to_string(param.ControllerNumber()));
                m_ValueEntry.set_text(param.InitialValue() ? std::to_string(param.InitialValue().value()) : "");
                m_LabelEntry.set_text(param.Label());
            }
        }
        ~TRow()
        {
            m_Destroying = true;
        }

        Gtk::Entry m_ControllerEntry;
        Gtk::Entry m_ValueEntry;
        Gtk::Entry m_LabelEntry;
        Gtk::Button m_RemoveButton;

    private:
        size_t m_Index;
        TEditParametersPanel* m_Parent;
        bool m_Destroying = false;
    };


    void on_remove_button_clicked(size_t index) {
        if (index < m_Parameters.size()) {
            m_Parameters.erase(m_Parameters.begin() + index);
            m_OnChange.emit();
            Update();  // Sync grid with the updated parameters
        }
    }


    std::vector<project::TInstrument::TParameter> m_Parameters;
    Gtk::ScrolledWindow m_ScrolledWindow;
    Gtk::VBox m_Box;
    Gtk::Grid m_Grid;
    Gtk::Button m_AddButton;
    std::vector<std::unique_ptr<TRow>> m_Rows;
    sigc::signal<void> m_OnChange;
    std::array<Gtk::Label,3> m_RowHeadersLabel {
        Gtk::Label("CC number"),
        Gtk::Label("Initial value"),
        Gtk::Label("Label"),
    };
};

class TEditPartDialog : public Gtk::Dialog {
public:
    TEditPartDialog(project::TPart &&part, std::vector<std::string> &&midiInPorts) : m_Part(std::move(part)), m_NameLabel("Name:"), m_MidiChanLabel("Midi channel (for Hammond):"),  m_OkButton("OK"), m_CancelButton("Cancel"), m_MidiInPortsLabel("Midi input:"), m_EditPortsPanel(std::move(midiInPorts), jackutils::PortKind::Midi, jackutils::PortDirection::Output)
    {
        // Set up the dialog properties
        set_title("Edit Part");
        set_modal(true);
        //set_default_size(400, 300);

        m_Grid.attach(m_NameLabel, 0, 0, 1, 1);
        m_Grid.attach(m_NameEntry, 1, 0, 1, 1);
        m_Grid.attach(m_MidiChanLabel, 0, 1, 1, 1);
        m_Grid.attach(m_MidiChanEntry, 1, 1, 1, 1);
        m_Grid.attach(m_MidiInPortsLabel, 0, 2, 1, 1);
        m_Grid.attach(m_EditPortsPanel, 1, 2, 1, 1);
        m_Grid.set_row_spacing(5);
        get_content_area()->add(m_Grid);
        m_Grid.set_hexpand(true);
        m_Grid.set_vexpand(true);
        m_NameLabel.set_halign(Gtk::ALIGN_START);
        m_MidiInPortsLabel.set_halign(Gtk::ALIGN_START);
        m_MidiChanLabel.set_halign(Gtk::ALIGN_START);
        m_NameEntry.set_hexpand(true);
        m_MidiChanEntry.set_hexpand(false);
        m_MidiChanEntry.set_halign(Gtk::ALIGN_START);
        m_MidiChanEntry.set_size_request(50, -1);
        m_NameEntry.set_size_request(80, -1);
        add_action_widget(m_CancelButton, Gtk::RESPONSE_CANCEL);
        add_action_widget(m_OkButton, Gtk::RESPONSE_OK);
        m_NameEntry.signal_changed().connect([this]() {
            dlg2data();
        });
        m_MidiChanEntry.signal_changed().connect([this]() {
            dlg2data();
        });
        m_MidiChanEntry.signal_focus_out_event().connect([this](GdkEventFocus* event) {
            data2dlg();
            return false;  // Continue with the default handler
        });
        m_NameEntry.signal_focus_out_event().connect([this](GdkEventFocus* event) {
            data2dlg();
            return false;  // Continue with the default handler
        });
        show_all_children();
        data2dlg();
    }
    ~TEditPartDialog()
    {
        m_Destroying = true;
    }
    const std::vector<std::string>& MidiInPorts() const
    {
        return m_EditPortsPanel.PortNames();
    }
    void data2dlg()
    {
        if(!m_Destroying) // we may get called from signal_focus_out_event() after destruction has begun
        {
            if(m_NameEntry.get_text() != m_Part.Name())
            {
                m_NameEntry.set_text(m_Part.Name());
            }
            auto chanstring = std::to_string(m_Part.MidiChannelForSharedInstruments() + 1);
            if(m_MidiChanEntry.get_text() != chanstring)
            {
                m_MidiChanEntry.set_text(chanstring);
            }
            m_OkButton.set_sensitive(!m_Part.Name().empty());
        }
    }
    void dlg2data()
    {
        m_Part = m_Part.ChangeName(utils::trim(m_NameEntry.get_text()));
        try
        {
            auto chan = utils::to_int64(m_MidiChanEntry.get_text());
            if( (chan >= 1) && (chan <= 16))
            {
                m_Part = m_Part.ChangeMidiChannelForSharedInstruments((int)chan - 1);
            }
        }
        catch(...) {}
        m_OkButton.set_sensitive(!m_Part.Name().empty());
    }
    const project::TPart &Part() const { return m_Part; }

private:
    project::TPart m_Part;
    Gtk::Grid m_Grid;
    Gtk::Label m_NameLabel;
    Gtk::Entry m_NameEntry;
    Gtk::Label m_MidiChanLabel;
    Gtk::Entry m_MidiChanEntry;
    Gtk::Label m_MidiInPortsLabel;
    Gtk::Button m_OkButton;
    Gtk::Button m_CancelButton;
    TEditPortsPanel m_EditPortsPanel;
    bool m_Destroying = false;
};

class TEditPresetDialog : public Gtk::Dialog {
public:
    TEditPresetDialog(project::TPreset &&preset, std::vector<project::TInstrument>&& instruments)
        : m_Preset(std::move(preset)), m_Instruments(std::move(instruments)),
        m_NameLabel("Name:"),
        m_InstrumentCaptionLabel("Instrument:"),
        m_ParametersPanel(m_Preset.OverrideParameters()? std::vector<project::TInstrument::TParameter>(*m_Preset.OverrideParameters()) : std::vector<project::TInstrument::TParameter>()),
        m_OverrideParametersCheckButton("Use custom controller settings"),
        m_NameEntry(),
        m_OkButton("OK"),
        m_CancelButton("Cancel") 
    {
        // Set up the dialog properties
        set_title("Edit Preset");
        set_modal(true);
        set_default_size(400, 300);

        m_NameEntry.set_text(m_Preset.Name());
        m_OverrideParametersCheckButton.set_active((bool)m_Preset.OverrideParameters());

        m_Grid.attach(m_NameLabel, 0, 0, 1, 1);
        m_Grid.attach(m_NameEntry, 1, 0, 1, 1);
        m_Grid.attach(m_InstrumentCaptionLabel, 0, 1, 1, 1);
        m_Grid.attach(m_InstrumentLabel, 1, 1, 1, 1);

        m_NameLabel.set_halign(Gtk::ALIGN_START);
        m_InstrumentCaptionLabel.set_halign(Gtk::ALIGN_START);
        m_InstrumentLabel.set_hexpand(true);
        m_NameEntry.set_hexpand(true);
        if(m_Preset.InstrumentIndex() < m_Instruments.size())
        {
            m_InstrumentLabel.set_label(m_Instruments.at(m_Preset.InstrumentIndex()).Name());
        }

        m_Grid.set_row_spacing(5);
        m_Grid.attach(m_OverrideParametersCheckButton, 0, 2, 2, 1);

        // Add the parameters panel to the grid
        m_Grid.attach(m_ParametersPanel, 0, 3, 2, 1);

        // Add the grid to the dialog's content area
        get_content_area()->add(m_Grid);

        m_Grid.set_hexpand(true);
        m_Grid.set_vexpand(true);

        // Set up the buttons
        add_action_widget(m_CancelButton, Gtk::RESPONSE_CANCEL);
        add_action_widget(m_OkButton, Gtk::RESPONSE_OK);

        m_NameEntry.signal_changed().connect([this]() {
            Update();
        });

        m_OverrideParametersCheckButton.signal_toggled().connect([this]() {
            Update();
        });

        m_ParametersPanel.onchange().connect([this]() {
            Update();
        });
        
        // Show all widgets
        m_Grid.show_all();
        m_OkButton.show();
        m_CancelButton.show();
        show_all_children(true);
        Update(); 
    }

    void Update()
    {
        m_Preset = m_Preset.ChangeName(utils::trim(m_NameEntry.get_text()));
        {
            if(m_OverrideParametersCheckButton.get_active())
            {
                m_Preset = m_Preset.ChangeOverrideParameters({m_ParametersPanel.Parameters()});
            }
            else
            {
                m_Preset = m_Preset.ChangeOverrideParameters(std::nullopt);
            }
        }
        try
        {
            if(m_Preset.Name().empty())
            {
                throw std::runtime_error("Enter a name");
            }
            m_ErrorLabel.set_text("");
            m_ErrorLabel.set_visible(false);
            m_OkButton.set_sensitive(true);
        }
        catch(std::exception &e)
        {
            m_ErrorLabel.set_text(e.what());
            m_ErrorLabel.set_visible(true);
            m_OkButton.set_sensitive(false);
        }
        m_ParametersPanel.Update(); // update visibility of the row headers
        m_ParametersPanel.set_visible((bool)m_Preset.OverrideParameters());
    }
    const project::TPreset &Preset() const {return m_Preset; }

private:
    project::TPreset m_Preset;
    std::vector<project::TInstrument> m_Instruments;
    Gtk::Grid m_Grid;
    Gtk::Label m_NameLabel;
    Gtk::Entry m_NameEntry;
    Gtk::Label m_ErrorLabel;
    Gtk::Label m_InstrumentCaptionLabel;
    Gtk::Label m_InstrumentLabel;
    Gtk::CheckButton m_OverrideParametersCheckButton;
    Gtk::Button m_OkButton;
    Gtk::Button m_CancelButton;
    TEditParametersPanel m_ParametersPanel;
};

class TEditInstrumentDialog : public Gtk::Dialog {
public:
    TEditInstrumentDialog(project::TInstrument &&instrument, bool canEditUri)
        : m_Instrument(std::move(instrument)),
        m_Lv2UriLabel("Plugin URI:"),
        m_NameLabel("Name:"),
        m_IsHammondCheckButton("Hammond organ mode"),
        m_HasVocoderInputCheckButton("Has vocoder input"),
        m_ParametersPanel(std::vector<project::TInstrument::TParameter>(m_Instrument.Parameters())),
        m_Lv2UriEntry(),
        m_NameEntry(),
        m_ChooseLv2UriButton("Choose"),
        m_OkButton("OK"),
        m_CancelButton("Cancel") 
    {
        // Set up the dialog properties
        set_title("Edit Instrument");
        set_modal(true);
        set_default_size(400, 300);

        // Initialize the entries with the current instrument data
        m_Lv2UriEntry.set_text(m_Instrument.Lv2Uri());
        m_Lv2UriEntry.set_editable(false);
        m_NameEntry.set_text(m_Instrument.Name());
        m_IsHammondCheckButton.set_active(m_Instrument.IsHammond());
        m_HasVocoderInputCheckButton.set_active(m_Instrument.HasVocoderInput());

        // Set up the grid layout
        m_Grid.attach(m_Lv2UriLabel, 0, 0, 1, 1);
        m_Grid.attach(m_Lv2UriEntry, 1, 0, 1, 1);
        m_Grid.attach(m_ChooseLv2UriButton, 2, 0, 1, 1);

        m_Lv2UriLabel.set_halign(Gtk::ALIGN_START);
        m_NameLabel.set_halign(Gtk::ALIGN_START);
        m_ControllersLabel.set_halign(Gtk::ALIGN_START);
        m_ChooseLv2UriButton.set_halign(Gtk::ALIGN_START);
        m_ChooseLv2UriButton.set_sensitive(canEditUri);
        m_Lv2UriEntry.set_hexpand(true);
        m_NameEntry.set_hexpand(true);

        m_Grid.attach(m_NameLabel, 0, 1, 1, 1);
        m_Grid.attach(m_NameEntry, 1, 1, 2, 1);

        m_Grid.set_row_spacing(5);
        m_Grid.attach(m_IsHammondCheckButton, 1, 2, 2, 1);
        m_Grid.attach(m_HasVocoderInputCheckButton, 1, 3, 2, 1);

        // Add the parameters panel to the grid
        m_Grid.attach(m_ControllersLabel, 0, 4, 1, 1);
        m_Grid.attach(m_ParametersPanel, 1, 4, 2, 1);
        m_Grid.attach(m_ErrorLabel, 0, 5, 3, 1);
        // make red:
        m_ErrorLabel.override_color(Gdk::RGBA("red"));

        // Add the grid to the dialog's content area
        get_content_area()->add(m_Grid);

        m_Grid.set_hexpand(true);
        m_Grid.set_vexpand(true);

        // Set up the buttons
        add_action_widget(m_CancelButton, Gtk::RESPONSE_CANCEL);
        add_action_widget(m_OkButton, Gtk::RESPONSE_OK);

        // Connect the "Choose" button to open the plugin selector dialog
        m_ChooseLv2UriButton.signal_clicked().connect([this]() {
            PluginSelectorDialog dialog(m_Instrument.Lv2Uri());
            int result = dialog.run();
            if (result == Gtk::RESPONSE_OK && dialog.SelectedPluginUri().has_value()) {
                m_Lv2UriEntry.set_text(dialog.SelectedPluginUri().value());
                Update();
            }
        });

        m_Lv2UriEntry.signal_changed().connect([this]() {
            Update();
        });

        m_NameEntry.signal_changed().connect([this]() {
            Update();
        });

        m_Lv2UriEntry.signal_activate().connect([this](){
            if(m_OkButton.get_sensitive())
            {
                response(Gtk::RESPONSE_OK);
            }
        });

        m_IsHammondCheckButton.signal_toggled().connect([this]() {
            Update();
        });

        m_HasVocoderInputCheckButton.signal_toggled().connect([this]() {
            Update();
        });

        m_ParametersPanel.onchange().connect([this]() {
            Update();
        });
        
        // Show all widgets
        m_Grid.show_all();
        m_OkButton.show();
        m_CancelButton.show();
        m_ParametersPanel.show();
        show_all_children(true);
        m_ParametersPanel.Update(); // update visibility of the row headers
        Update();
    }

    const project::TInstrument &Instrument() const {
        return m_Instrument;
    }

    void Update()
    {
        m_Instrument = m_Instrument.ChangeHasVocoderInput(m_HasVocoderInputCheckButton.get_active()).ChangeIsHammond(m_IsHammondCheckButton.get_active()).ChangeLv2Uri(m_Lv2UriEntry.get_text()).ChangeName(utils::trim(m_NameEntry.get_text())).ChangeParameters(std::vector<project::TInstrument::TParameter>{m_ParametersPanel.Parameters()});
        try
        {
            if(m_Instrument.Lv2Uri().empty())
            {
                throw std::runtime_error("Select a plugin");
            }
            if(m_Instrument.Name().empty())
            {
                throw std::runtime_error("Enter a name");
            }
            try
            {
                lilvutils::Plugin plugin(lilvutils::Uri(std::string(m_Instrument.Lv2Uri())));
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error("Cannot instantiate plugin: "+std::string(e.what()));
            }
            m_ErrorLabel.set_text("");
            m_ErrorLabel.set_visible(false);
            m_OkButton.set_sensitive(true);
        }
        catch(std::exception &e)
        {
            m_ErrorLabel.set_text(e.what());
            m_ErrorLabel.set_visible(true);
            m_OkButton.set_sensitive(false);
        }
    }

private:
    project::TInstrument m_Instrument;
    Gtk::Grid m_Grid;
    Gtk::Label m_Lv2UriLabel;
    Gtk::Label m_ControllersLabel {"Controllers:"};
    Gtk::Label m_NameLabel;
    Gtk::Entry m_Lv2UriEntry;
    Gtk::Entry m_NameEntry;
    Gtk::Label m_ErrorLabel;
    Gtk::CheckButton m_IsHammondCheckButton;
    Gtk::CheckButton m_HasVocoderInputCheckButton;
    Gtk::Button m_ChooseLv2UriButton;
    Gtk::Button m_OkButton;
    Gtk::Button m_CancelButton;
    TEditParametersPanel m_ParametersPanel;
};

class PresetSelectorDialog : public Gtk::Dialog
{
public:
    PresetSelectorDialog(PresetSelectorDialog &&other) = delete;
    PresetSelectorDialog(const PresetSelectorDialog &other) = delete;
    PresetSelectorDialog(const project::TProject &project) : m_Project(project)
    {
        set_title("Select Preset");
        set_default_size(400, 300);
        add_button("Cancel", Gtk::RESPONSE_CANCEL);
        m_OkButton = add_button("OK", Gtk::RESPONSE_OK);
        m_PresetListScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        m_PresetListScrolledWindow.add(m_PresetList);

// Add a text box for entering the name of the preset
        //m_PresetNameEntry.set_text("Enter preset name");
        get_vbox()->set_spacing(5);
        get_vbox()->pack_start(m_PresetListScrolledWindow, Gtk::PACK_EXPAND_WIDGET);

        get_vbox()->pack_start(m_PresetNameEntry, Gtk::PACK_SHRINK);

        //m_PresetList.set_selection_mode(Gtk::SELECTION_SINGLE);
        m_PresetList.signal_cursor_changed().connect([this](){
            Gtk::TreeModel::Path path;
            Gtk::TreeViewColumn* focus_column;
            m_PresetList.get_cursor(path, focus_column);
            const project::TPreset *preset = nullptr;
            if(path)
            {
                auto iter = m_ListStore->get_iter(path);
                if(iter)
                {
                    m_SelectedPreset = iter->get_value(m_Columns.m_Number);
                    if(m_SelectedPreset && *m_SelectedPreset < m_Project.Presets().size())
                    {
                        if(m_Project.Presets()[*m_SelectedPreset])
                        {
                            preset = &m_Project.Presets()[*m_SelectedPreset].value();
                        }
                    }

                }
            }
            if(preset)
            {
                m_PresetNameEntry.set_text(preset->Name());
            }
            else
            {
                m_PresetNameEntry.set_text("");
            }
            enableItems();
        });
        m_PresetList.set_model(m_ListStore);
        m_PresetList.append_column("Number", m_Columns.m_Number);
        m_PresetList.append_column("Name", m_Columns.m_Name);
        show_all();
        m_PresetNameEntry.signal_changed().connect([this](){
            enableItems();
        });
        Populate();
        enableItems();
    }
    void enableItems()
    {
        bool canok = true;
        if(m_PresetNameEntry.get_text().empty())
        {
            canok = false;
        }
        if(!m_SelectedPreset)
        {
            canok = false;
        }
        m_OkButton->set_sensitive(canok);
    }
    std::optional<size_t> SelectedPreset() const { return m_SelectedPreset; }
    std::string PresetName() const { return m_PresetNameEntry.get_text(); }
    void Populate()
    {
        m_ListStore->clear();
        size_t numpresets = std::max((size_t)128, m_Project.Presets().size());
        for(size_t i = 0; i < numpresets; i++)
        {
            const project::TPreset *preset = nullptr;
            if(i < m_Project.Presets().size())
            {
                if(m_Project.Presets()[i])
                {
                    preset = &m_Project.Presets()[i].value();
                }
            }
            Gtk::TreeModel::Row row = *(m_ListStore->append());
            row[m_Columns.m_Number] = i;
            row[m_Columns.m_Name] = preset ? preset->Name() : "(empty)";
        }
    }
private:
    class Columns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Columns()
        {
            add(m_Number);
            add(m_Name);
        }
        Gtk::TreeModelColumn<size_t> m_Number;
        Gtk::TreeModelColumn<std::string> m_Name;
    };
    const project::TProject &m_Project;
    Gtk::ScrolledWindow m_PresetListScrolledWindow;
    std::optional<size_t> m_SelectedPreset;
    Columns m_Columns;
    Glib::RefPtr<Gtk::ListStore> m_ListStore {Gtk::ListStore::create(m_Columns)};
    Gtk::TreeView m_PresetList;
    Gtk::Entry m_PresetNameEntry;
    Gtk::Button *m_OkButton = nullptr;
};

class ApplicationWindow : public Gtk::ApplicationWindow
{
public:
    enum class FocusedTab { Presets, Instruments, Reverb };
    class PartsContainer : public Gtk::Box
    {
    public:
        class PartContainer : public Gtk::Box
        {
        public:
            PartContainer(PartContainer &&other) = delete;
            PartContainer(const PartContainer &other) = delete;
            PartContainer(engine::Engine &engine, size_t partindex) : m_PartIndex(partindex), m_Engine(engine)
            {
                set_orientation(Gtk::ORIENTATION_VERTICAL);
                set_vexpand(false);
                pack_start(m_ActivateButton);
                m_ActivateButton.set_size_request(100, 100);
                m_ActivateButton.signal_clicked().connect([this](){
                    DoAndShowException([this](){
                        if(m_PartIndex < m_Engine.Data().Project().Parts().size())
                        {
                            bool isSelected = m_Engine.Data().GuiFocusedPart() == m_PartIndex;
                            if(m_Engine.Data().GuiFocusedPart() != m_PartIndex)
                            {
                                auto newdata = m_Engine.Data().ChangeGuiFocusedPart(m_PartIndex);
                                m_Engine.SetData(std::move(newdata));
                            }
                            else
                            {
                                // still focused:
                                m_ActivateButton.set_state_flags(Gtk::STATE_FLAG_CHECKED);
                            }
                        }
                    });
                });
                pack_start(m_LevelSlider, Gtk::PACK_SHRINK);
                m_LevelSlider.signal_value_changed().connect([this](){
                    DoAndShowException([this](){
                        auto v = m_LevelSlider.Multiplier();
                        m_Engine.SetProject(m_Engine.Project().ChangePart(m_PartIndex,
                        m_Engine.Project().Parts().at(m_PartIndex).ChangeAmplitudeFactor((float)v)));
                    });
                });
                show_all_children();
                OnDataChanged();
            }
            void OnDataChanged()
            {
                bool isSelected = m_Engine.Data().GuiFocusedPart() == m_PartIndex;
                if(m_PartIndex < m_Engine.Data().Project().Parts().size())
                {
                    const auto &part = m_Engine.Data().Project().Parts()[m_PartIndex];
                    m_ActivateButton.set_label(part.Name());
                    m_ActivateButton.set_state_flags(isSelected ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
                    m_LevelSlider.SetMultiplier(m_Engine.Project().Parts().at(m_PartIndex).AmplitudeFactor());
                }
            }
        private:
            Gtk::ToggleButton m_ActivateButton;
            engine::Engine &m_Engine;
            size_t m_PartIndex;
            utils::NotifySink m_OnDataChanged {m_Engine.OnDataChanged(), [this](){OnDataChanged();}};
            TLevelSlider m_LevelSlider;
        };
        void EditPartWithDialog(size_t partindex)
        {
            auto part = (partindex < m_Engine.Project().Parts().size())? m_Engine.Project().Parts().at(partindex) : project::TPart("Part " + std::to_string(partindex + 1));
            std::vector<std::string> midiInPorts;
            if(partindex < m_Engine.Data().JackConnections().MidiInputs().size())
            {
                midiInPorts = m_Engine.Data().JackConnections().MidiInputs().at(partindex);
            }
            TEditPartDialog dlg(std::move(part), std::move(midiInPorts));
            int result = dlg.run();
            if(result == Gtk::RESPONSE_OK)
            {
                auto modifiedpart =  (partindex < m_Engine.Project().Parts().size())? m_Engine.Data().Project().Parts().at(partindex).ChangeName(std::string{dlg.Part().Name()}).ChangeMidiChannelForSharedInstruments(dlg.Part().MidiChannelForSharedInstruments()) : dlg.Part();
                auto newproject = (partindex < m_Engine.Project().Parts().size())? m_Engine.Data().Project().ChangePart(m_Engine.Data().GuiFocusedPart().value(), std::move(modifiedpart)) : m_Engine.Data().Project().AddPart(std::move(modifiedpart));
                auto midiinputs = m_Engine.Data().JackConnections().MidiInputs();
                if(midiinputs.size() <= partindex)
                {
                    midiinputs.resize(partindex + 1);
                }
                midiinputs[partindex] = {};
                for(const auto &port: dlg.MidiInPorts())
                {
                    if(!port.empty())
                    {
                        midiinputs[partindex].push_back(port);
                    }
                }
                auto newjackconnections = m_Engine.Data().JackConnections().ChangeMidiInputs(std::move(midiinputs));
                auto newdata = m_Engine.Data().ChangeProject(std::move(newproject)).ChangeJackConnections(std::move(newjackconnections));
                m_Engine.SetData(std::move(newdata));
            }
        }
        PartsContainer(engine::Engine &engine) : m_Engine(engine), Gtk::Box(Gtk::ORIENTATION_HORIZONTAL)
        {
            set_spacing(5);
            pack_start(m_ScrolledWindow, Gtk::PACK_SHRINK);
            pack_start(m_MenuBox, Gtk::PACK_SHRINK);
            m_ScrolledWindow.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
            m_ScrolledWindow.add(m_PartsBox);
            m_PartsBox.set_valign(Gtk::ALIGN_START);
            m_MenuBox.pack_start(m_MenuButton, Gtk::PACK_SHRINK);
            m_PartsBox.set_orientation(Gtk::ORIENTATION_VERTICAL);
            m_PartsBox.set_spacing(20);
            m_PopupMenu.append(m_EditPartMenuItem);
            m_PopupMenu.append(m_DeletePartMenuItem);
            m_PopupMenu.append(m_AddPartMenuItem);
            m_PopupMenu.show_all();
            m_MenuButton.set_popup(m_PopupMenu);
            m_MenuButton.set_image(m_MenuButtonImage);
            m_EditPartMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    size_t partindex = m_Engine.Data().GuiFocusedPart().value();
                    EditPartWithDialog(partindex);
                });
            });
            m_DeletePartMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    size_t partindex = m_Engine.Data().GuiFocusedPart().value();
                    auto partname = m_Engine.Project().Parts().at(partindex).Name();
                    std::string msg = "Delete part '"+partname+"'?";
                    Gtk::MessageDialog dialog(msg, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
                    int result = dialog.run();
                    if(result == Gtk::RESPONSE_YES)
                    {
                        auto newproject = m_Engine.Project().DeletePart(partindex);
                        m_Engine.SetProject(std::move(newproject));
                    }
                });
            });
            m_AddPartMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    auto partindex = m_Engine.Project().Parts().size();
                    EditPartWithDialog(partindex);
                });
            });

            show_all_children();
            Update();
        }
        void Update()
        {
            const auto &project = m_Engine.Project();
            while(m_PartContainers.size() > project.Parts().size())
            {
                m_PartContainers.pop_back();
            }
            while(m_PartContainers.size() < project.Parts().size())
            {
                auto container = std::make_unique<PartContainer>(m_Engine, m_PartContainers.size());
                container->set_valign(Gtk::ALIGN_START);
                container->set_vexpand(false);
                m_PartContainers.push_back(std::move(container));
                m_PartsBox.pack_start(*m_PartContainers.back());
            }
            m_PartsBox.show_all_children();
            m_DeletePartMenuItem.set_sensitive(m_Engine.Data().GuiFocusedPart() && m_Engine.Project().Parts().size() > 1);
            m_EditPartMenuItem.set_sensitive((bool)m_Engine.Data().GuiFocusedPart());
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnDataChanged {m_Engine.OnDataChanged(), [this](){Update();}};
        std::vector<std::unique_ptr<PartContainer>> m_PartContainers;
        Gtk::ScrolledWindow m_ScrolledWindow;
        Gtk::Box m_PartsBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::Box m_MenuBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::MenuButton m_MenuButton;
        Gtk::Image m_MenuButtonImage {"open-menu-symbolic", Gtk::ICON_SIZE_MENU};
        Gtk::Menu m_PopupMenu;
        Gtk::MenuItem m_DeletePartMenuItem {"Delete active part"};
        Gtk::MenuItem m_EditPartMenuItem{"Edit active part"};
        Gtk::MenuItem m_AddPartMenuItem{"Add part"};
    };
    class ReverbPanel : public Gtk::Box
    {
    public:
        ReverbPanel(ReverbPanel &&other) = delete;
        ReverbPanel(const ReverbPanel &other) = delete;
        ReverbPanel(engine::Engine &engine) : m_Engine(engine), Gtk::Box(Gtk::ORIENTATION_HORIZONTAL)
        {
            pack_start(m_Grid, Gtk::PACK_SHRINK);
            pack_start(m_MenuButton, Gtk::PACK_SHRINK);
            m_Grid.attach(m_ReverbNameCaptionLabel, 0, 0, 1, 1);
            m_Grid.attach(m_ReverbNameLabel, 1, 0, 1, 1);
            m_Grid.attach(m_ReverbLevelCaptionLabel, 0, 1, 1, 1);
            m_Grid.attach(m_ReverbLevelSlider, 1, 1, 1, 1);
            m_Grid.set_row_spacing(5);
            m_Grid.set_column_spacing(5);
            m_Grid.set_border_width(5);
            m_PopupMenu.append(m_ChangePluginButton);
            m_PopupMenu.append(m_ReverbStorePresetButton);
            m_PopupMenu.append(m_ShowGuiMenuItem);
            m_PopupMenu.show_all();
            m_MenuButton.set_valign(Gtk::ALIGN_START);
            m_MenuButton.set_popup(m_PopupMenu);
            m_MenuButton.set_image(m_MenuButtonImage);
            m_ReverbLevelSlider.set_halign(Gtk::ALIGN_START);
            m_ShowGuiMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    auto newdata = m_Engine.Data().ChangeShowReverbUi(!m_Engine.Data().ShowReverbUi());
                    m_Engine.SetData(std::move(newdata));
                    m_Engine.LoadReverbPreset();
                });
            });
            m_ChangePluginButton.signal_activate().connect([this](){
                DoAndShowException([this](){
                    PluginSelectorDialog dialog(m_Engine.Project().Reverb().ReverbLv2Uri());
                    int result = dialog.run();
                    if(result == Gtk::RESPONSE_OK)
                    {
                        auto reverburi = dialog.SelectedPluginUri().value();
                        m_Engine.ChangeReverbLv2Uri(std::move(reverburi));
                    }
                });
            });
            m_ReverbStorePresetButton.signal_activate().connect([this](){
                DoAndShowException([this](){
                    m_Engine.StoreReverbPreset();
                });
            });
            m_ReverbLevelSlider.signal_value_changed().connect([this](){
                DoAndShowException([this](){
                    auto reverb = m_Engine.Project().Reverb().ChangeMixLevel(m_ReverbLevelSlider.Multiplier());
                    auto project = m_Engine.Project().ChangeReverb(std::move(reverb));
                    m_Engine.SetProject(std::move(project));
                });
            });
            OnDataChanged();
        }
        void OnDataChanged()
        {
            const auto &project = m_Engine.Project();
            m_ShowGuiMenuItem.set_state_flags(m_Engine.Data().ShowReverbUi()?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ReverbNameLabel.set_text(m_Engine.ReverbPluginName());
            show_all_children();
            m_ReverbLevelSlider.SetMultiplier(project.Reverb().MixLevel());
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnDataChanged {m_Engine.OnDataChanged(), [this](){OnDataChanged();}};
        Gtk::Grid m_Grid;
        Gtk::Label m_ReverbNameCaptionLabel {"Plugin:", Gtk::ALIGN_START};
        Gtk::Label m_ReverbLevelCaptionLabel {"Mix level:", Gtk::ALIGN_START};
        Gtk::Label m_ReverbNameLabel {"", Gtk::ALIGN_START};
        Gtk::MenuButton m_MenuButton;
        Gtk::Image m_MenuButtonImage {"open-menu-symbolic", Gtk::ICON_SIZE_MENU};
        Gtk::Menu m_PopupMenu;
        Gtk::MenuItem m_ChangePluginButton {"Change reverb plugin"};
        Gtk::MenuItem m_ReverbStorePresetButton {"Store reverb preset"};
        Gtk::CheckMenuItem m_ShowGuiMenuItem {"Show reverb GUI"};
        TLevelSlider m_ReverbLevelSlider;
    };


    class PresetsPanel : public Gtk::Box
    {
    public:
        PresetsPanel(PresetsPanel &&other) = delete;
        PresetsPanel(const PresetsPanel &other) = delete;
        PresetsPanel(engine::Engine &engine) : m_Engine(engine), Gtk::Box(Gtk::ORIENTATION_HORIZONTAL)
        {
            m_PresetsScrolledWindow.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
            m_PresetsBox.pack_start(m_PresetsFlowBox, Gtk::PACK_SHRINK);
            m_PresetsScrolledWindow.add(m_PresetsBox);
            pack_start(m_PresetsScrolledWindow, Gtk::PACK_EXPAND_WIDGET);
            pack_start(m_MenuButtonBox, Gtk::PACK_SHRINK);
            m_MenuButtonBox.pack_start(m_MenuButton, Gtk::PACK_SHRINK);
            m_PopupMenu.append(m_SavePresetMenuItem);
            m_PopupMenu.append(m_DeletePresetMenuItem);
            m_PopupMenu.append(m_EditPresetMenuItem);
            m_PopupMenu.append(m_ShowGuiMenuItem);
            m_PopupMenu.show_all();
            m_MenuButton.set_popup(m_PopupMenu);
            m_MenuButton.set_image(m_MenuButtonImage);
            m_EditPresetMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    auto presetindex = m_Engine.Project().Parts().at(m_Engine.Data().GuiFocusedPart().value()).ActivePresetIndex().value();
                    auto preset = m_Engine.Project().Presets().at(presetindex).value();
                    auto instruments = m_Engine.Project().Instruments();
                    TEditPresetDialog dialog(std::move(preset), std::move(instruments));
                    int result = dialog.run();
                    if(result == Gtk::RESPONSE_OK)
                    {
                        auto preset = dialog.Preset();
                        m_Engine.SetProject(m_Engine.Project().ChangePreset(presetindex, std::move(preset)));
                    }
                });
            });

            m_SavePresetMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    auto project_copy = m_Engine.Project();
                    PresetSelectorDialog dialog(project_copy);
                    int result = dialog.run();
                    if(result == Gtk::RESPONSE_OK)
                    {
                        if(dialog.SelectedPreset())
                        {
                            if(m_Engine.Data().GuiFocusedPart())
                            {
                                m_Engine.SaveCurrentPreset(*m_Engine.Data().GuiFocusedPart(), *dialog.SelectedPreset(), dialog.PresetName());
                            }
                        }
                    }
                });
            });
            m_DeletePresetMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    if(m_Engine.Data().GuiFocusedPart() && (*m_Engine.Data().GuiFocusedPart() < m_Engine.Project().Parts().size()))
                    {
                        auto presetindex = m_Engine.Project().Parts().at(*m_Engine.Data().GuiFocusedPart()).ActivePresetIndex();
                        if(presetindex && (*presetindex < m_Engine.Project().Presets().size()))
                        {
                            const auto &currentpreset = m_Engine.Project().Presets().at(*presetindex);
                            if(currentpreset)
                            {
                                std::string msg = "Delete preset "+std::to_string(*presetindex)+" ("+currentpreset.value().Name()+")?";
                                Gtk::MessageDialog dialog(msg, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
                                int result = dialog.run();
                                if(result == Gtk::RESPONSE_YES)
                                {
                                    m_Engine.DeletePreset(*presetindex);
                                }
                            }
                        }
                    }
                });
            });

            m_ShowGuiMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    auto newdata = m_Engine.Data().ChangeShowUi(!m_Engine.Data().ShowUi());
                    m_Engine.SetData(std::move(newdata));
                });
            });

            OnDataChanged();
        }
        void enableItems()
        {
            bool hasActivePreset = false;
            bool canSavePreset = false;
            if(m_Engine.Data().GuiFocusedPart() && (*m_Engine.Data().GuiFocusedPart() < m_Engine.Project().Parts().size()))
            {
                auto presetindex = m_Engine.Project().Parts().at(*m_Engine.Data().GuiFocusedPart()).ActivePresetIndex();
                if(presetindex && (*presetindex < m_Engine.Project().Presets().size()))
                {
                    const auto &currentpreset = m_Engine.Project().Presets().at(*presetindex);
                    if(currentpreset)
                    {
                        hasActivePreset = true;
                    }
                }
                auto instrindex = m_Engine.Project().Parts().at(*m_Engine.Data().GuiFocusedPart()).ActiveInstrumentIndex();
                if(instrindex)
                {
                    canSavePreset = true;
                }
            }
            m_DeletePresetMenuItem.set_sensitive(hasActivePreset);
            m_EditPresetMenuItem.set_sensitive(hasActivePreset);
            m_SavePresetMenuItem.set_sensitive(canSavePreset);
        }
        void OnDataChanged()
        {
            std::optional<size_t> activePreset;
            if(m_Engine.Data().GuiFocusedPart() && *m_Engine.Data().GuiFocusedPart() < m_Engine.Data().Project().Parts().size())
            {
                const auto &part = m_Engine.Data().Project().Parts()[*m_Engine.Data().GuiFocusedPart()];
                activePreset = part.ActivePresetIndex();
            }

            std::vector<std::unique_ptr<Gtk::ToggleButton>> presetButtons;
            std::vector<size_t> btnIndex2PresetIndex;
            bool needRelayout = false;

            for(size_t presetIndex = 0; presetIndex < m_Engine.Data().Project().Presets().size(); presetIndex++)
            {
                const auto &preset = m_Engine.Data().Project().Presets()[presetIndex];
                if(preset)
                {
                    auto btnIndex = btnIndex2PresetIndex.size();
                    btnIndex2PresetIndex.push_back(presetIndex);
                    std::unique_ptr<Gtk::ToggleButton> button;
                    if(m_PresetButtons.size() > presetButtons.size())
                    {
                        button = std::move(m_PresetButtons[presetButtons.size()]);
                    }
                    else
                    {
                        button = std::make_unique<Gtk::ToggleButton>();
                        m_PresetsFlowBox.add(*button);
                        button->signal_clicked().connect([this, btnIndex](){
                            DoAndShowException([this, btnIndex](){
                                auto presetIndex = m_BtnIndex2PresetIndex.at(btnIndex);
                                if(m_Engine.Data().GuiFocusedPart() && *m_Engine.Data().GuiFocusedPart() < m_Engine.Data().Project().Parts().size())
                                {
                                    auto newproject = m_Engine.Project().SwitchToPreset(*m_Engine.Data().GuiFocusedPart(), presetIndex);
                                    m_Engine.SetProject(std::move(newproject));
                                }
                            });
                        });
                        button->set_size_request(100, 100);
                    }
                    auto label = std::to_string(presetIndex) + " " + preset.value().Name();
                    if(button->get_label() != label)
                    {
                        button->set_label(label);
                    }
                    button->set_state_flags(activePreset && *activePreset == presetIndex ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
                    presetButtons.push_back(std::move(button));
                    needRelayout = true;
                }
            }
            for(size_t presetIndex = m_Engine.Data().Project().Presets().size(); presetIndex < m_PresetButtons.size(); presetIndex++)
            {
                m_PresetsFlowBox.remove(*m_PresetButtons[presetIndex]);
                needRelayout = true;
            }
            m_PresetButtons = std::move(presetButtons);
            m_BtnIndex2PresetIndex = std::move(btnIndex2PresetIndex);
            if(needRelayout)
            {
                show_all_children();
            }
            m_ShowGuiMenuItem.set_state_flags(m_Engine.Data().ShowUi()?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            enableItems();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnDataChanged {m_Engine.OnDataChanged(), [this](){OnDataChanged();}};
        Gtk::ScrolledWindow m_PresetsScrolledWindow;
        Gtk::Box m_PresetsBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::FlowBox m_PresetsFlowBox;
        std::vector<std::unique_ptr<Gtk::ToggleButton>> m_PresetButtons;
        Gtk::Box m_MenuButtonBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::MenuButton m_MenuButton;
        Gtk::Image m_MenuButtonImage {"open-menu-symbolic", Gtk::ICON_SIZE_MENU};
        Gtk::Menu m_PopupMenu;
        Gtk::MenuItem m_SavePresetMenuItem {"Save preset"};
        Gtk::MenuItem m_DeletePresetMenuItem {"Delete current preset"};
        Gtk::MenuItem m_EditPresetMenuItem{"Edit current preset"};
        Gtk::CheckMenuItem m_ShowGuiMenuItem{"Show plugin GUI"};
        std::vector<size_t> m_BtnIndex2PresetIndex;
    };
    class InstrumentsPanel : public Gtk::Box
    {
    public:
        InstrumentsPanel(InstrumentsPanel &&other) = delete;
        InstrumentsPanel(const InstrumentsPanel &other) = delete;
        InstrumentsPanel(engine::Engine &engine) : m_Engine(engine), Gtk::Box(Gtk::ORIENTATION_HORIZONTAL)
        {
            m_InstrumentsScrolledWindow.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
            m_InstrumentsBox.pack_start(m_InstrumentsFlowBox, Gtk::PACK_SHRINK);
            m_InstrumentsScrolledWindow.add(m_InstrumentsBox);
            pack_start(m_InstrumentsScrolledWindow, Gtk::PACK_EXPAND_WIDGET);
            pack_start(m_MenuButtonBox, Gtk::PACK_SHRINK);
            m_MenuButtonBox.pack_start(m_MenuButton, Gtk::PACK_SHRINK);
            m_PopupMenu.append(m_AddInstrumentItem);
            m_PopupMenu.append(m_EditInstrumentItem);
            m_PopupMenu.append(m_DeleteInstrumentItem);
            m_PopupMenu.append(m_ShowGuiMenuItem);
            m_PopupMenu.show_all();
            m_MenuButton.set_popup(m_PopupMenu);
            m_MenuButton.set_image(m_MenuButtonImage);
            m_AddInstrumentItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    project::TInstrument instrument({}, false, {}, {}, false);
                    TEditInstrumentDialog dlg(std::move(instrument), true);
                    int result = dlg.run();
                    if(result == Gtk::RESPONSE_OK)
                    {
                        auto newinstrument = dlg.Instrument();
                        auto newproject = m_Engine.Project().AddInstrument(std::move(newinstrument));
                        m_Engine.SetProject(std::move(newproject));
                    }
                });
            });
            m_ShowGuiMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    auto newdata = m_Engine.Data().ChangeShowUi(!m_Engine.Data().ShowUi());
                    m_Engine.SetData(std::move(newdata));
                });
            });
            m_EditInstrumentItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    if(m_Engine.Data().GuiFocusedPart())
                    {
                        auto activeinstrumentindexOrNull = m_Engine.Project().Parts().at(m_Engine.Data().GuiFocusedPart().value()).ActiveInstrumentIndex();
                        if(activeinstrumentindexOrNull)
                        {
                            auto instrumentindex = activeinstrumentindexOrNull.value();
                            auto instrument = m_Engine.Project().Instruments().at(activeinstrumentindexOrNull.value());
                            TEditInstrumentDialog dlg(std::move(instrument), false);
                            int result = dlg.run();
                            if(result == Gtk::RESPONSE_OK)
                            {
                                auto newinstrument = dlg.Instrument();
                                auto newproject = m_Engine.Project().ChangeInstrument(instrumentindex, std::move(newinstrument));
                                m_Engine.SetProject(std::move(newproject));
                            }
                        }
                    }
                });
            });
            m_DeleteInstrumentItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    if(m_Engine.Data().GuiFocusedPart() && (*m_Engine.Data().GuiFocusedPart() < m_Engine.Project().Parts().size()))
                    {
                        auto instrumentindex = m_Engine.Project().Parts().at(*m_Engine.Data().GuiFocusedPart()).ActiveInstrumentIndex();
                        if(instrumentindex && (*instrumentindex < m_Engine.Project().Instruments().size()))
                        {
                            std::string msg = "Delete instrument "+std::to_string(*instrumentindex)+" ("+m_Engine.Project().Instruments()[*instrumentindex].Name()+")?";
                            Gtk::MessageDialog dialog(msg, false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);
                            int result = dialog.run();
                            if(result == Gtk::RESPONSE_YES)
                            {
                                m_Engine.DeleteInstrument(*instrumentindex);
                            }
                        }
                    }
                });
            });
            OnDataChanged();
        }
        void enableItems()
        {
            bool hasselected = false;
            if(m_Engine.Data().GuiFocusedPart() && (*m_Engine.Data().GuiFocusedPart() < m_Engine.Project().Parts().size()))
            {
                auto instrumentindex = m_Engine.Project().Parts().at(*m_Engine.Data().GuiFocusedPart()).ActiveInstrumentIndex();
                if(instrumentindex && (*instrumentindex < m_Engine.Project().Instruments().size()))
                {
                    hasselected = true;
                }
            }
            m_DeleteInstrumentItem.set_sensitive(hasselected);
            m_EditInstrumentItem.set_sensitive(hasselected);
        }
        void OnDataChanged()
        {
            const auto &project = m_Engine.Project();
            std::optional<size_t> activeInstrument;
            if(m_Engine.Data().GuiFocusedPart() && *m_Engine.Data().GuiFocusedPart() < project.Parts().size())
            {
                const auto &part = project.Parts()[*m_Engine.Data().GuiFocusedPart()];
                activeInstrument = part.ActiveInstrumentIndex();
            }

            while(m_InstrumentButtons.size() > project.Instruments().size())
            {
                m_InstrumentsFlowBox.remove(*m_InstrumentButtons.back());
                m_InstrumentButtons.pop_back();
            }
            while(m_InstrumentButtons.size() < project.Instruments().size())
            {
                auto button = std::make_unique<Gtk::ToggleButton>();
                size_t instrumentindex = m_InstrumentButtons.size();
                button->signal_clicked().connect([this, instrumentindex](){
                    DoAndShowException([this, instrumentindex](){
                        const auto &project = m_Engine.Project();
                        if(m_Engine.Data().GuiFocusedPart() && (*m_Engine.Data().GuiFocusedPart() < project.Parts().size()))
                        {
                            const auto &part = project.Parts()[*m_Engine.Data().GuiFocusedPart()];
                            if(part.ActiveInstrumentIndex() != instrumentindex)
                            {
                                auto newpart = part.ChangeActiveInstrumentIndex(instrumentindex).ChangeActivePresetIndex(std::nullopt);

                                auto newproject = project.ChangePart(*m_Engine.Data().GuiFocusedPart(), std::move(newpart));
                                m_Engine.SetProject(std::move(newproject));
                            }
                        }
                        OnDataChanged();
                    });
                });
                button->set_size_request(100, 100);
                m_InstrumentButtons.push_back(std::move(button));
                m_InstrumentsFlowBox.add(*m_InstrumentButtons.back());
            }
            for(size_t i = 0; i < m_InstrumentButtons.size(); i++)
            {
                const auto &instrument = project.Instruments()[i];
                auto &button = *m_InstrumentButtons[i];
                button.set_label(instrument.Name());
                button.set_state_flags(activeInstrument && *activeInstrument == i ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            }
            m_ShowGuiMenuItem.set_state_flags(m_Engine.Data().ShowUi()?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            show_all_children();
            enableItems();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnDataChanged {m_Engine.OnDataChanged(), [this](){OnDataChanged();}};
        Gtk::FlowBox m_InstrumentsFlowBox;
        Gtk::Box m_InstrumentsBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::ScrolledWindow m_InstrumentsScrolledWindow;
        std::vector<std::unique_ptr<Gtk::ToggleButton>> m_InstrumentButtons;
        Gtk::Box m_MenuButtonBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::MenuButton m_MenuButton;
        Gtk::Image m_MenuButtonImage {"open-menu-symbolic", Gtk::ICON_SIZE_MENU};
        Gtk::Menu m_PopupMenu;
        Gtk::MenuItem m_AddInstrumentItem {"Add Instrument"};
        Gtk::MenuItem m_DeleteInstrumentItem {"Delete Instrument"};
        Gtk::MenuItem m_EditInstrumentItem {"Edit Instrument"};
        Gtk::CheckMenuItem m_ShowGuiMenuItem{"Show plugin GUI"};
    };
    class TopBar : public Gtk::Box
    {
    public:
        TopBar(TopBar&&) = delete;
        TopBar(ApplicationWindow &applicationWindow) : m_ApplicationWindow(applicationWindow)
        {
            set_orientation(Gtk::ORIENTATION_HORIZONTAL);
            m_ModePresetButton.signal_clicked().connect([this](){
                m_ApplicationWindow.SetFocusedTab(FocusedTab::Presets);
                Update();
            });
            m_ModeInstrumentsButton.signal_clicked().connect([this](){
                m_ApplicationWindow.SetFocusedTab(FocusedTab::Instruments);
                Update();
            });
            m_ModeReverbButton.signal_clicked().connect([this](){
                m_ApplicationWindow.SetFocusedTab(FocusedTab::Reverb);
                Update();
            });
            m_MenuButton.set_popup(applicationWindow.PopupMenu());
            m_MenuButton.set_image(m_MenuButtonImage);
            pack_start(m_ModePresetButton, Gtk::PACK_SHRINK);
            pack_start(m_ModeInstrumentsButton, Gtk::PACK_SHRINK);
            pack_start(m_ModeReverbButton, Gtk::PACK_SHRINK);
            pack_end(m_MenuButton, Gtk::PACK_SHRINK);
            Update();
        }
        void Update()
        {
            auto focusedTab = m_ApplicationWindow.focusedTab();
            m_ModePresetButton.set_state_flags(focusedTab == FocusedTab::Presets?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ModeInstrumentsButton.set_state_flags(focusedTab == FocusedTab::Instruments?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ModeReverbButton.set_state_flags(focusedTab == FocusedTab::Reverb?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
        }
    private:
        Gtk::ToggleButton m_ModePresetButton {"Presets"};
        Gtk::ToggleButton m_ModeInstrumentsButton {"Instruments"};
        Gtk::ToggleButton m_ModeReverbButton {"Reverb"};
        Gtk::MenuButton m_MenuButton;
        Gtk::Image m_MenuButtonImage {"open-menu-symbolic", Gtk::ICON_SIZE_MENU};
        ApplicationWindow &m_ApplicationWindow;
        utils::NotifySink m_OnDataChanged {m_ApplicationWindow.Engine().OnDataChanged(), [this](){Update();}};
    };
    ApplicationWindow(engine::Engine &engine) : m_Engine(engine), m_PartsContainer(engine)
    {
        m_CssProvider->load_from_data(R"VVV(
.redbg {
    background-color: #ff0000; 
}
)VVV");
        set_default_size(800, 600);
        m_Box1.set_spacing(10);
        add(m_Box1);
        m_Box1.pack_start(*m_TopBar, Gtk::PACK_SHRINK);
        m_Box1.pack_start(m_Box2, Gtk::PACK_EXPAND_WIDGET);
        m_Box2.pack_start(m_MainPanelStack, Gtk::PACK_EXPAND_WIDGET);
        m_Box2.pack_start(m_Spacer1, Gtk::PACK_SHRINK);
        m_Spacer1.set_size_request(20, -1);
        m_Box2.pack_start(m_PartsContainer, Gtk::PACK_SHRINK);
        m_PartsContainer.set_valign(Gtk::ALIGN_FILL);        
        m_MainPanelStack.add(*m_PresetsPanel);
        m_MainPanelStack.add(*m_InstrumentsPanel);
        m_MainPanelStack.add(*m_ReverbPanel);
        show_all_children();
        m_SettingsMenuItem.signal_activate().connect([this](){
            DoAndShowException([this](){
                auto jackconnections = m_Engine.Data().JackConnections();
                TEditSettingsDialog dlg(std::move(jackconnections));
                int result = dlg.run();
                if(result == Gtk::RESPONSE_OK)
                {
                    auto newjackconnections = dlg.JackConnections();
                    m_Engine.SetData(m_Engine.Data().ChangeJackConnections(std::move(newjackconnections)));
                }
            });
        });
        m_PopupMenu.append(m_SettingsMenuItem);
        m_PopupMenu.show_all();
        Update();
    }
    void Update()
    {
        if(m_FocusedTab == FocusedTab::Presets)
        {
            m_MainPanelStack.set_visible_child(*m_PresetsPanel);
        }
        else if(m_FocusedTab == FocusedTab::Instruments)
        {
            m_MainPanelStack.set_visible_child(*m_InstrumentsPanel);
        }
        else if(m_FocusedTab == FocusedTab::Reverb)
        {
            m_MainPanelStack.set_visible_child(*m_ReverbPanel);
        }
    }
    const FocusedTab& focusedTab() const { return m_FocusedTab; }
    void SetFocusedTab(const FocusedTab& focusedTab) 
    { 
        m_FocusedTab = focusedTab;
        m_TopBar->Update();
        Update();
    }
    engine::Engine& Engine() { return m_Engine; }
    Gtk::Menu& PopupMenu() { return m_PopupMenu; }

private:
    FocusedTab m_FocusedTab = FocusedTab::Presets;
    engine::Engine &m_Engine;
    utils::NotifySink m_OnDataChanged {m_Engine.OnDataChanged(), [this](){Update();}};
    PartsContainer m_PartsContainer;
    Gtk::Box m_Box1 {Gtk::ORIENTATION_VERTICAL};
    Gtk::Box m_Box2 {Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Box m_Spacer1 {Gtk::ORIENTATION_VERTICAL};
    Gtk::Menu m_PopupMenu;
    Gtk::MenuItem m_SettingsMenuItem {"Settings..."};
    std::unique_ptr<TopBar> m_TopBar {std::make_unique<TopBar>(*this)};
    std::unique_ptr<PresetsPanel> m_PresetsPanel { std::make_unique<PresetsPanel>(m_Engine) };
    std::unique_ptr<InstrumentsPanel> m_InstrumentsPanel { std::make_unique<InstrumentsPanel>(m_Engine) };
    std::unique_ptr<ReverbPanel> m_ReverbPanel { std::make_unique<ReverbPanel>(m_Engine) };
    Gtk::Stack m_MainPanelStack;
    Glib::RefPtr<Gtk::CssProvider> m_CssProvider {Gtk::CssProvider::create()};
};

} // anonymous namespace

Gtk::ApplicationWindow* guiCreateApplicationWindow(engine::Engine &engine)
{
    return new ApplicationWindow(engine);
}

