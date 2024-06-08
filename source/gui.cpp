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

class PluginSelectorDialog : public Gtk::Dialog
{
public:
    PluginSelectorDialog(PluginSelectorDialog &&other) = delete;
    PluginSelectorDialog(const PluginSelectorDialog &other) = delete;
    PluginSelectorDialog(bool showSharedCheckbox)
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
        if(showSharedCheckbox)
        {
            get_vbox()->pack_start(m_SharedCheckbox, Gtk::PACK_SHRINK);
        }
        Populate();
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
    bool Shared() const
    {
        return m_SharedCheckbox.get_active();
    }

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
    Gtk::CheckButton m_SharedCheckbox {"Add as shared instrument"};
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
                pack_start(m_ActivateButton);
                m_ActivateButton.signal_clicked().connect([this](){
                    DoAndShowException([this](){
                        const auto &prj = m_Engine.Project();
                        if(m_PartIndex < prj.Parts().size())
                        {
                            bool isSelected = prj.FocusedPart() == m_PartIndex;
                            if(prj.FocusedPart() != m_PartIndex)
                            {
                                auto newproject = prj.Change(m_PartIndex, prj.ShowUi());
                                m_Engine.SetProject(std::move(newproject));
                            }
                            else
                            {
                                // still focused:
                                m_ActivateButton.set_state_flags(Gtk::STATE_FLAG_CHECKED);
                            }
                        }
                    });
                });
                OnProjectChanged();
            }
            void OnProjectChanged()
            {
                const auto &prj = m_Engine.Project();
                bool isSelected = prj.FocusedPart() == m_PartIndex;
                if(m_PartIndex < prj.Parts().size())
                {
                    const auto &part = prj.Parts()[m_PartIndex];
                    m_ActivateButton.set_label(part.Name());
                    m_ActivateButton.set_state_flags(isSelected ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
                }
            }
        private:
            Gtk::ToggleButton m_ActivateButton;
            engine::Engine &m_Engine;
            size_t m_PartIndex;
            utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        };
        PartsContainer(engine::Engine &engine) : m_Engine(engine)
        {
            set_orientation(Gtk::ORIENTATION_VERTICAL);
            OnProjectChanged();
        }
        void OnProjectChanged()
        {
            const auto &project = m_Engine.Project();
            while(m_PartContainers.size() > project.Parts().size())
            {
                remove(*m_PartContainers.back());
                m_PartContainers.pop_back();
            }
            while(m_PartContainers.size() < project.Parts().size())
            {
                auto container = std::make_unique<PartContainer>(m_Engine, m_PartContainers.size());
                m_PartContainers.push_back(std::move(container));
                pack_start(*m_PartContainers.back());
            }
            show_all_children();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        std::vector<std::unique_ptr<PartContainer>> m_PartContainers;
    };
    class ReverbPanel : public Gtk::ScrolledWindow
    {
    public:
        ReverbPanel(ReverbPanel &&other) = delete;
        ReverbPanel(const ReverbPanel &other) = delete;
        ReverbPanel(engine::Engine &engine) : m_Engine(engine)
        {
            set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
            add(m_MainBox);
            m_MainBox.set_spacing(5);
            m_ButtonsBox.pack_start(m_ShowGuiButton, Gtk::PACK_SHRINK);
            m_ButtonsBox.pack_start(m_ChangePluginButton, Gtk::PACK_SHRINK);
            m_ButtonsBox.pack_start(m_ReverbStorePresetButton, Gtk::PACK_SHRINK);
            m_MainBox.pack_start(m_ButtonsBox, Gtk::PACK_SHRINK);
            m_MainBox.pack_start(m_ReverbNameLabel, Gtk::PACK_SHRINK);
            m_MainBox.pack_start(m_ReverbLevelScale, Gtk::PACK_SHRINK);
            m_ShowGuiButton.signal_clicked().connect([this](){
                DoAndShowException([this](){
                    auto reverb = m_Engine.Project().Reverb().ChangeShowGui(!m_Engine.Project().Reverb().ShowGui());
                    auto project = m_Engine.Project().ChangeReverb(std::move(reverb));
                    m_Engine.SetProject(std::move(project));

                    m_Engine.LoadReverbPreset();
                });
            });
            m_ChangePluginButton.signal_clicked().connect([this](){
                DoAndShowException([this](){
                    PluginSelectorDialog dialog(false);
                    int result = dialog.run();
                    if(result == Gtk::RESPONSE_OK)
                    {
                        auto reverburi = dialog.SelectedPluginUri().value();
                        m_Engine.ChangeReverbLv2Uri(std::move(reverburi));
                    }
                });
            });
            m_ReverbStorePresetButton.signal_clicked().connect([this](){
                DoAndShowException([this](){
                    m_Engine.StoreReverbPreset();
                });
            });
            m_ReverbLevelScale.signal_value_changed().connect([this](){
                DoAndShowException([this](){
                    if(!m_ChangingReverbLevel)
                    {
                        m_ChangingReverbLevel = true;
                        utils::finally finally([this](){m_ChangingReverbLevel = false;});
                        auto reverb = m_Engine.Project().Reverb().ChangeMixLevel(m_ReverbLevelScale.get_value());
                        auto project = m_Engine.Project().ChangeReverb(std::move(reverb));
                        m_Engine.SetProject(std::move(project));
                    }
                });
            });
            m_ReverbLevelScale.set_digits(2);
            OnProjectChanged();
        }
        void OnProjectChanged()
        {
            const auto &project = m_Engine.Project();
            m_ShowGuiButton.set_state_flags(project.Reverb().ShowGui()?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ReverbNameLabel.set_text(m_Engine.ReverbPluginName());
            show_all_children();
            m_ReverbLevelScale.set_range(0.0, 1.0);
            if(!m_ChangingReverbLevel)
            {
                m_ChangingReverbLevel = true;
                utils::finally finally([this](){m_ChangingReverbLevel = false;});
                m_ReverbLevelScale.set_value(project.Reverb().MixLevel());
            }
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        Gtk::Box m_MainBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::Box m_ButtonsBox {Gtk::ORIENTATION_HORIZONTAL};
        Gtk::ToggleButton m_ShowGuiButton {"Show GUI"};
        Gtk::Label m_ReverbNameLabel;
        Gtk::Button m_ChangePluginButton {"Change Plugin"};
        Gtk::Button m_ReverbStorePresetButton {"Store Preset"};
        Gtk::Scale m_ReverbLevelScale;
        bool m_ChangingReverbLevel = false;
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
            m_PopupMenu.show_all();
            m_MenuButton.set_popup(m_PopupMenu);
            m_SavePresetMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    auto project_copy = m_Engine.Project();
                    PresetSelectorDialog dialog(project_copy);
                    int result = dialog.run();
                    if(result == Gtk::RESPONSE_OK)
                    {
                        if(dialog.SelectedPreset())
                        {
                            m_Engine.SaveCurrentPreset(*dialog.SelectedPreset(), dialog.PresetName());
                        }
                    }
                });
            });
            m_DeletePresetMenuItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    if(m_Engine.Project().FocusedPart() && (*m_Engine.Project().FocusedPart() < m_Engine.Project().Parts().size()))
                    {
                        auto presetindex = m_Engine.Project().Parts().at(*m_Engine.Project().FocusedPart()).ActivePresetIndex();
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
            OnProjectChanged();
        }
        void enableItems()
        {
            bool canDeletePreset = false;
            bool canSavePreset = false;
            if(m_Engine.Project().FocusedPart() && (*m_Engine.Project().FocusedPart() < m_Engine.Project().Parts().size()))
            {
                auto presetindex = m_Engine.Project().Parts().at(*m_Engine.Project().FocusedPart()).ActivePresetIndex();
                if(presetindex && (*presetindex < m_Engine.Project().Presets().size()))
                {
                    const auto &currentpreset = m_Engine.Project().Presets().at(*presetindex);
                    if(currentpreset)
                    {
                        canDeletePreset = true;
                    }
                }
                auto instrindex = m_Engine.Project().Parts().at(*m_Engine.Project().FocusedPart()).ActiveInstrumentIndex();
                if(instrindex)
                {
                    canSavePreset = true;
                }
            }
            m_DeletePresetMenuItem.set_sensitive(canDeletePreset);
            m_SavePresetMenuItem.set_sensitive(canSavePreset);
        }
        void OnProjectChanged()
        {
            const auto &project = m_Engine.Project();
            std::optional<size_t> activePreset;
            if(project.FocusedPart() && *project.FocusedPart() < project.Parts().size())
            {
                const auto &part = project.Parts()[*project.FocusedPart()];
                activePreset = part.ActivePresetIndex();
            }

            std::vector<std::unique_ptr<Gtk::ToggleButton>> presetButtons;
            for(const auto &b: m_PresetButtons)
            {
                m_PresetsFlowBox.remove(*b);
            }
            m_PresetButtons.clear();
            for(size_t presetIndex = 0; presetIndex < project.Presets().size(); presetIndex++)
            {
                const auto &preset = project.Presets()[presetIndex];
                if(preset)
                {
                    std::unique_ptr<Gtk::ToggleButton> button;
                    //if(m_PresetButtons.size() > presetButtons.size())
                    if(false)
                    {
                        button = std::move(m_PresetButtons[presetButtons.size()]);
                    }
                    else
                    {
                        button = std::make_unique<Gtk::ToggleButton>();
                        m_PresetsFlowBox.add(*button);
                    }
                    // set minimum height:
                    button->set_size_request(100, 100);
                    button->set_label(std::to_string(presetIndex) + " " + preset.value().Name());
                    button->set_state_flags(activePreset && *activePreset == presetIndex ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
                    button->signal_clicked().connect([this, presetIndex](){
                        DoAndShowException([this, presetIndex](){
                            m_Engine.SwitchFocusedPartToPreset(presetIndex);
                            OnProjectChanged();
                        });
                    });
                    presetButtons.push_back(std::move(button));
                }
            }
            m_PresetButtons = std::move(presetButtons);
            show_all_children();
            enableItems();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        Gtk::ScrolledWindow m_PresetsScrolledWindow;
        Gtk::Box m_PresetsBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::FlowBox m_PresetsFlowBox;
        std::vector<std::unique_ptr<Gtk::ToggleButton>> m_PresetButtons;
        Gtk::Box m_MenuButtonBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::MenuButton m_MenuButton;
        Gtk::Menu m_PopupMenu;
        Gtk::MenuItem m_SavePresetMenuItem {"Save Preset"};
        Gtk::MenuItem m_DeletePresetMenuItem {"Delete Current Preset"};
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
            m_PopupMenu.append(m_DeleteInstrumentItem);
            m_PopupMenu.show_all();
            m_MenuButton.set_popup(m_PopupMenu);
            add(m_InstrumentsFlowBox);
            m_AddInstrumentItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    PluginSelectorDialog dialog(true);
                    int result = dialog.run();
                    if(result == Gtk::RESPONSE_OK)
                    {
                        auto instrumenturi = dialog.SelectedPluginUri().value();
                        m_Engine.AddInstrument(std::move(instrumenturi), dialog.Shared());
                    }
                });
            });
            m_DeleteInstrumentItem.signal_activate().connect([this](){
                DoAndShowException([this](){
                    if(m_Engine.Project().FocusedPart() && (*m_Engine.Project().FocusedPart() < m_Engine.Project().Parts().size()))
                    {
                        auto instrumentindex = m_Engine.Project().Parts().at(*m_Engine.Project().FocusedPart()).ActiveInstrumentIndex();
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
            OnProjectChanged();
        }
        void enableItems()
        {
            bool candelete = false;
            if(m_Engine.Project().FocusedPart() && (*m_Engine.Project().FocusedPart() < m_Engine.Project().Parts().size()))
            {
                auto instrumentindex = m_Engine.Project().Parts().at(*m_Engine.Project().FocusedPart()).ActiveInstrumentIndex();
                if(instrumentindex && (*instrumentindex < m_Engine.Project().Instruments().size()))
                {
                    candelete = true;
                }
            }
            m_DeleteInstrumentItem.set_sensitive(candelete);
        }
        void OnProjectChanged()
        {
            const auto &project = m_Engine.Project();
            std::optional<size_t> activeInstrument;
            if(project.FocusedPart() && *project.FocusedPart() < project.Parts().size())
            {
                const auto &part = project.Parts()[*project.FocusedPart()];
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
                        if(project.FocusedPart() && (*project.FocusedPart() < project.Parts().size()))
                        {
                            const auto &part = project.Parts()[*project.FocusedPart()];
                            if(part.ActiveInstrumentIndex() != instrumentindex)
                            {
                                auto newpart = part.ChangeActiveInstrumentIndex(instrumentindex).ChangeActivePresetIndex(std::nullopt);

                                auto newproject = project.ChangePart(*project.FocusedPart(), std::move(newpart));
                                m_Engine.SetProject(std::move(newproject));
                            }
                        }
                        OnProjectChanged();
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
            show_all_children();
            enableItems();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        Gtk::FlowBox m_InstrumentsFlowBox;
        Gtk::Box m_InstrumentsBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::ScrolledWindow m_InstrumentsScrolledWindow;
        std::vector<std::unique_ptr<Gtk::ToggleButton>> m_InstrumentButtons;
        Gtk::Box m_MenuButtonBox {Gtk::ORIENTATION_VERTICAL};
        Gtk::MenuButton m_MenuButton;
        Gtk::Menu m_PopupMenu;
        Gtk::MenuItem m_AddInstrumentItem {"Add Instrument"};
        Gtk::MenuItem m_DeleteInstrumentItem {"Delete Instrument"};
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
            m_ShowGuiButton.signal_clicked().connect([this](){
                DoAndShowException([this](){
                    auto prj = m_ApplicationWindow.Engine().Project().Change(m_ApplicationWindow.Engine().Project().FocusedPart(), !m_ApplicationWindow.Engine().Project().ShowUi());
                    m_ApplicationWindow.Engine().SetProject(std::move(prj));
                });
            });
            m_MenuButton.set_popup(applicationWindow.PopupMenu());
            pack_start(m_ModePresetButton, Gtk::PACK_SHRINK);
            pack_start(m_ModeInstrumentsButton, Gtk::PACK_SHRINK);
            pack_start(m_ModeReverbButton, Gtk::PACK_SHRINK);
            pack_start(m_ShowGuiButton, Gtk::PACK_SHRINK);
            pack_start(m_MenuButton, Gtk::PACK_SHRINK);
            Update();
        }
        void Update()
        {
            auto focusedTab = m_ApplicationWindow.focusedTab();
            m_ModePresetButton.set_state_flags(focusedTab == FocusedTab::Presets?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ModeInstrumentsButton.set_state_flags(focusedTab == FocusedTab::Instruments?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ModeReverbButton.set_state_flags(focusedTab == FocusedTab::Reverb?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ShowGuiButton.set_state_flags(m_ApplicationWindow.Engine().Project().ShowUi()?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
        }
    private:
        Gtk::ToggleButton m_ModePresetButton {"Presets"};
        Gtk::ToggleButton m_ModeInstrumentsButton {"Instruments"};
        Gtk::ToggleButton m_ModeReverbButton {"Reverb"};
        Gtk::ToggleButton m_ShowGuiButton {"GUI"};
        Gtk::MenuButton m_MenuButton;
        ApplicationWindow &m_ApplicationWindow;
        utils::NotifySink m_OnProjectChanged {m_ApplicationWindow.Engine().OnProjectChanged(), [this](){Update();}};
    };
    ApplicationWindow(engine::Engine &engine) : m_Engine(engine), m_PartsContainer(engine)
    {
        set_default_size(800, 600);
        add(m_Box1);
        m_Box1.pack_start(*m_TopBar, Gtk::PACK_SHRINK);
        m_Box1.pack_start(m_Box2, Gtk::PACK_EXPAND_WIDGET);
        m_Box2.pack_start(m_MainPanelStack, Gtk::PACK_EXPAND_WIDGET);
        m_Box2.pack_start(m_PartsContainer, Gtk::PACK_SHRINK);
        m_MainPanelStack.add(*m_PresetsPanel);
        m_MainPanelStack.add(*m_InstrumentsPanel);
        m_MainPanelStack.add(*m_ReverbPanel);
        show_all_children();
        m_SaveProjectMenuItem.signal_activate().connect([this](){
            DoAndShowException([this](){
                m_Engine.SaveProject();
            });
        });
        m_PopupMenu.append(m_SaveProjectMenuItem);
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
    utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){Update();}};
    PartsContainer m_PartsContainer;
    Gtk::Box m_Box1 {Gtk::ORIENTATION_VERTICAL};
    Gtk::Box m_Box2 {Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Menu m_PopupMenu;
    std::unique_ptr<TopBar> m_TopBar {std::make_unique<TopBar>(*this)};
    std::unique_ptr<PresetsPanel> m_PresetsPanel { std::make_unique<PresetsPanel>(m_Engine) };
    std::unique_ptr<InstrumentsPanel> m_InstrumentsPanel { std::make_unique<InstrumentsPanel>(m_Engine) };
    std::unique_ptr<ReverbPanel> m_ReverbPanel { std::make_unique<ReverbPanel>(m_Engine) };
    Gtk::Stack m_MainPanelStack;
    Gtk::MenuItem m_SaveProjectMenuItem {"Save Project"};
};

} // anonymous namespace

Gtk::ApplicationWindow* guiCreateApplicationWindow(engine::Engine &engine)
{
    return new ApplicationWindow(engine);
}

