#include "stdafx.h"

#include "ActorEditor.h"

#include "ActorEditorListCtrl.h"
#include "AtlasObject/AtlasObject.h"
#include "AtlasObject/AtlasObjectText.h"
#include "General/Datafile.h"

#include "wx/file.h"

BEGIN_EVENT_TABLE(ActorEditor, AtlasWindow)
	EVT_MENU(ID_CreateEntity, OnCreateEntity)
END_EVENT_TABLE()


ActorEditor::ActorEditor(wxWindow* parent)
	: AtlasWindow(parent, _("Actor Editor"), wxSize(1024, 450))
{
	SetIcon(wxIcon(_T("ICON_ActorEditor")));

	wxMenu* menu = new wxMenu;
	menu->Append(ID_CreateEntity, _("Create &entity..."));
	AddCustomMenu(menu, _("&Actor"));

	//////////////////////////////////////////////////////////////////////////
	
	wxPanel* mainPanel = new wxPanel(this);

	m_ActorEditorListCtrl = new ActorEditorListCtrl(mainPanel);

	wxBoxSizer* vertSizer = new wxBoxSizer(wxVERTICAL);
	mainPanel->SetSizer(vertSizer);

	wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
	vertSizer->Add(topSizer,
		wxSizerFlags().Border(wxLEFT|wxRIGHT, 5));

	vertSizer->Add(
		m_ActorEditorListCtrl,
		wxSizerFlags().Proportion(1).Expand().Border(wxALL, 10));

	//////////////////////////////////////////////////////////////////////////
	// Properties panel:

	wxPanel* propertiesPanel = new wxPanel(mainPanel);
	topSizer->Add(propertiesPanel, wxSizerFlags().Expand().Border(wxLEFT|wxRIGHT, 5));

	wxSizer* propertiesSizer = new wxStaticBoxSizer(
		new wxStaticBox(propertiesPanel, wxID_ANY, _("Actor properties")),
		wxHORIZONTAL);

	propertiesPanel->SetSizer(propertiesSizer);


	m_CastShadows = new wxCheckBox(propertiesPanel, wxID_ANY, _("Cast shadow"));
	propertiesSizer->Add(m_CastShadows, wxSizerFlags().Border(wxALL, 5));

	// TODO: Orientation property.

	//////////////////////////////////////////////////////////////////////////
	// Materials box:

	wxPanel* materialsPanel = new wxPanel(mainPanel);
	topSizer->Add(materialsPanel, wxSizerFlags().Expand().Border(wxLEFT|wxRIGHT, 5));

	wxSizer* materialsSizer = new wxStaticBoxSizer(
		new wxStaticBox(materialsPanel, wxID_ANY, _("Material")),
		wxHORIZONTAL);

	materialsPanel->SetSizer(materialsSizer);

	// Get the list of XML materials
	wxArrayString materials = Datafile::EnumerateDataFiles(_T("mods/official/art/materials/"), _T("*.xml"));
	// Extract the filenames and discard the path
	for (size_t i = 0; i < materials.Count(); ++i)
		materials[i] = wxFileName(materials[i]).GetFullName();

	m_Material = new wxComboBox(materialsPanel, wxID_ANY, _T(""), wxDefaultPosition, wxDefaultSize, materials);
	materialsSizer->Add(m_Material, wxSizerFlags().Border(wxALL, 2));

	//////////////////////////////////////////////////////////////////////////

//	void convert_actors();
//	convert_actors();
}

void ActorEditor::ThawData(AtObj& in)
{
	AtObj actor (in["actor"]);
	m_ActorEditorListCtrl->ThawData(actor);

	if (actor["castshadow"].defined())
		m_CastShadows->SetValue(true);
	else
		m_CastShadows->SetValue(false);

	m_Material->SetValue(actor["material"]);
}

AtObj ActorEditor::FreezeData()
{
	AtObj actor (m_ActorEditorListCtrl->FreezeData());

	if (m_CastShadows->IsChecked())
		actor.set("castshadow", L"");

	if (m_Material->GetValue().length())
		actor.set("material", m_Material->GetValue().c_str());

	AtObj out;
	out.set("actor", actor);
	return out;
}


static AtObj ConvertToLatestFormat(AtObj in)
{
	if (! in.defined())
	{
		// 'Importing' a new blank file. Fill it in with default values:
		AtObj actor;
		actor.add("@version", L"1");
		in.add("actor", actor);
	}

	// Determine the file format version
	long version;

	if (in["Object"].defined())
	{
		// old-style actor format
		version = -1;
	}
	else if (in["actor"].defined())
	{
		if (in["actor"]["@version"].defined())
			wxString(in["actor"]["@version"]).ToLong(&version);
		else
			version = 0;
	}
	else
	{
		wxLogError(_("Failed to determine actor file format version"));
		return AtObj();
	}


	// Do any necessary conversions into the most recent format:

	if (version == -1)
	{
		AtObj actor;

		// Handle the global actor properties
		if (wxString(in["Object"]["Properties"]["@autoflatten"]) == _T("1"))
			actor.add("autoflatten", L"");

		if (wxString(in["Object"]["Properties"]["@castshadows"]) == _T("1"))
			actor.add("castshadow", L"");

		// Things to strip leading strings (for converting filenames, since the
		// new format doesn't store the entire path)
		#define THING1(out,outname, in,inname, prefix) \
			wxASSERT( wxString(in["Object"][inname]).StartsWith(_T(prefix)) ); \
			out.add(outname, wxString(in["Object"][inname]).Mid(wxString(_T(prefix)).Length()))
		#define THING2(out,outname, in,inname, prefix) \
			wxASSERT( wxString(in[inname]).StartsWith(_T(prefix)) ); \
			out.add(outname, wxString(in[inname]).Mid(wxString(_T(prefix)).Length()))

		if (wxString(in["Object"]["Material"]).Len())
		{
			THING1(actor,"material", in,"Material", "art/materials/");
		}

		// Create a single variant to contain all the old data
		AtObj var;
		var.add("@name", L"Base");
		var.add("@frequency", L"100"); // 100 == default frequency

		THING1(var,"mesh",    in,"ModelName",   "art/meshes/");

		// XXX
		if (wxString(in["Object"]["TextureName"]).StartsWith(_T("art/textures/ui/session/portraits/ui_portrait_sheet_civ_")))
		{
			var.add("texture", L"temp/" + wxString(in["Object"]["TextureName"]).Mid(strlen("art/textures/ui/session/portraits/")));
		}
		else
		{
			THING1(var,"texture", in,"TextureName", "art/textures/skins/");
		}

		AtObj anims;
		for (AtIter animit = in["Object"]["Animations"]["Animation"]; animit.defined(); ++animit)
		{
			if (wcslen(animit["@file"]))
			{
				AtObj anim;
				anim.add("@name", animit["@name"]);
				anim.add("@speed", animit["@speed"]);

				THING2(anim,"@file", animit,"@file",  "art/animation/");

				anims.add("animation", anim);
			}
		}
		var.add("animations", anims);

		AtObj props;
		for (AtIter propit = in["Object"]["Props"]["Prop"]; propit.defined(); ++propit)
		{
			AtObj prop;
			prop.add("@attachpoint", propit["@attachpoint"]);
			prop.add("@actor", propit["@model"]);
			props.add("prop", prop);
		}
		var.add("props", props);

		AtObj grp;
		grp.add("variant", var);
		actor.add("group", grp);

		#undef THING1
		#undef THING2

		actor.set("@version", L"1");
		in = AtObj();
		in.set("actor", actor);
	}
	else if (version == 0)
	{
		AtObj actor;

		if (in["actor"]["castshadow"].defined()) actor.add("castshadow", in["actor"]["castshadow"]);
		if (in["actor"]["material"].defined()) actor.add("material", in["actor"]["material"]);

		for (AtIter grpit = in["actor"]["group"]; grpit.defined(); ++grpit)
		{
			AtObj grp;
			for (AtIter varit = grpit["variant"]; varit.defined(); ++varit)
			{
				AtObj var;
				var.add("@name", varit["name"]);
				var.add("@frequency", varit["frequency"]);
				var.add("mesh", varit["mesh"]);
				var.add("texture", varit["texture"]);

				AtObj anims;
				for (AtIter animit = varit["animations"]["animation"]; animit.defined(); ++animit)
				{
					AtObj anim;
					anim.add("@name", animit["name"]);
					anim.add("@file", animit["file"]);
					anim.add("@speed", animit["speed"]);

					anims.add("animation", anim);
				}
				var.add("animations", anims);

				AtObj props;
				for (AtIter propit = varit["props"]["prop"]; propit.defined(); ++propit)
				{
					AtObj prop;
					prop.add("@attachpoint", propit["attachpoint"]);
					prop.add("@actor", propit["model"]);

					props.add("prop", prop);
				}
				var.add("props", props);

				grp.add("variant", var);
			}
			actor.add("group", grp);
		}

		actor.set("@version", L"1");
		in.set("actor", actor);
	}
	else if (version == 1)
	{
		// current format
	}
	else
	{
		// ??? unknown format - this should have been noticed earlier
		wxFAIL_MSG(_T("Invalid actor format"));
	}

	return in;
}


void ActorEditor::ImportData(AtObj& in)
{
	AtObj data = ConvertToLatestFormat(in);
	if (! data.defined())
		return;

	// Copy the data into the appropriate UI controls:

	AtObj actor (data["actor"]);
	m_ActorEditorListCtrl->ImportData(actor);

	if (actor["castshadow"].defined())
		m_CastShadows->SetValue(true);
	else
		m_CastShadows->SetValue(false);

	m_Material->SetValue(actor["material"]);
}

AtObj ActorEditor::ExportData()
{
	// Export the group/variant/etc data
	AtObj actor (m_ActorEditorListCtrl->ExportData());

	actor.set("@version", L"1");

	if (m_CastShadows->IsChecked())
		actor.set("castshadow", L"");

	if (m_Material->GetValue().length())
		actor.set("material", m_Material->GetValue().c_str());

	AtObj out;
	out.set("actor", actor);
	return out;
}


void ActorEditor::OnCreateEntity(wxCommandEvent& WXUNUSED(event))
{
	// Create a very basic entity for this actor.
	
	// The output should be an XML file like:
	//
	//   <?xml version="1.0" encoding="iso-8859-1" standalone="no"?>
	//   <Entity Parent="celt_csw_b">
	//     <Actor>units/celt_csw_a.xml</Actor>
	//   </Entity>
	//
	// 'Actor' comes from this actor's filename.
	// 'Parent' comes from the filename of a user-selected entity.
	// The file will be saved into a user-selected location.
	
	// Get the entity's expected name
	wxFileName currentFilename (GetCurrentFilename());
	if (! currentFilename.IsOk())
	{
		wxMessageDialog(this, _("Please save this actor before attempting to create an entity for it."),
			_("Gentle reminder"), wxOK | wxICON_INFORMATION).ShowModal();
		return;
	}
	wxString entityName = currentFilename.GetName();

	// Work out where the entities are stored
	wxFileName entityPath (_T("mods/official/entities/"));
	entityPath.MakeAbsolute(Datafile::GetDataDirectory());

	// Make sure the user knows what's going on
	static bool instructed = false; // only tell them once per session
	if (! instructed)
	{
		instructed = true;
		if (wxMessageBox(_("To create an entity, you will first be asked to choose a parent entity, and then asked where save the new entity."),
			_("Usage instructions"), wxICON_INFORMATION|wxOK|wxCANCEL, this) != wxOK)
			return; // cancelled by user
	}

	wxString parentEntityFilename
		(wxFileSelector(_("Choose a parent entity"), entityPath.GetPath(), _T(""),
		_T("xml"), _("XML files (*.xml)|*.xml|All files (*.*)|*.*"), wxOPEN, this));

	if (! parentEntityFilename.Length())
		return; // cancelled by user

	// Get the parent's name
	wxString parentName (wxFileName(parentEntityFilename).GetName());

	wxString outputEntityFilename
		(wxFileSelector(_("Choose a filename to save as"), entityPath.GetPath(), entityName,
		_T("xml"), _("XML files (*.xml)|*.xml|All files (*.*)|*.*"), wxSAVE|wxOVERWRITE_PROMPT, this));

	if (! outputEntityFilename.Length())
		return; // cancelled by user

	// Get this actor's filename, relative to actors/
	wxFileName actorPath (_T("mods/official/art/actors/"));
	actorPath.MakeAbsolute(Datafile::GetDataDirectory());
	wxFileName actorFilename (currentFilename);
	actorFilename.MakeRelativeTo(actorPath.GetFullPath());

	// Create the XML data to be written
	// TODO: Native line endings
	wxString xml =
		_T("<?xml version=\"1.0\" encoding=\"iso-8859-1\" standalone=\"no\"?>\r\n")
		_T("\r\n")
		_T("<Entity Parent=\"") + parentName + _T("\">\r\n")
		_T("\t<Actor>") + actorFilename.GetFullPath(wxPATH_UNIX) + _T("</Actor>\r\n")
		_T("</Entity>\r\n");

	wxFile file (outputEntityFilename.fn_str(), wxFile::write);
	if (! file.IsOpened())
	{
		wxLogError(_("Failed to open file"));
		return;
	}

	if (! file.Write(xml))
	{
		wxLogError(_("Failed to write XML data to file"));
		return;
	}
}

wxString ActorEditor::GetDefaultOpenDirectory()
{
	wxFileName dir (_T("mods/official/art/actors/"), wxPATH_UNIX);
	dir.MakeAbsolute(Datafile::GetDataDirectory());
	return dir.GetPath();
}


#if 0
// TODO: delete this
void merge_actors(const char* newFn, const char* oldFn_b, const char* oldFn_a, const char* oldFn_e)
{
wxFAIL;
	/*
	wxLogDebug(L"Converting %s -> %s", wxString::FromAscii(oldFn_b), wxString::FromAscii(newFn));

	AtObj obj0 = AtlasObject::LoadFromXML(L"e:/0ad/svnc/trunk/binaries/data/mods/official/art/actors_clean/" + wxString::FromAscii(oldFn_b));
	AtObj obj1 = AtlasObject::LoadFromXML(L"e:/0ad/svnc/trunk/binaries/data/mods/official/art/actors_clean/" + wxString::FromAscii(oldFn_a));
	AtObj obj2 = AtlasObject::LoadFromXML(L"e:/0ad/svnc/trunk/binaries/data/mods/official/art/actors_clean/" + wxString::FromAscii(oldFn_e));
	wxASSERT(obj0.defined());
	wxASSERT(obj2.defined());
	wxASSERT(obj1.defined());
	obj0 = ConvertToLatestFormat(obj0);
	obj1 = ConvertToLatestFormat(obj1);
	obj2 = ConvertToLatestFormat(obj2);
//	wxString x = AtlasObject::ConvertToString(obj).c_str();
//	wxLogDebug(x);
	bool ok = AtlasObject::SaveToXML(obj, L"e:/0ad/svnc/trunk/binaries/data/mods/official/art/actors_new/" + wxString::FromAscii(newFn));
	wxASSERT(ok);
*/
}

void convert_file(const char* newFn, const char* oldFn)
{
//*
	wxLogDebug(L"Converting %s -> %s", wxString::FromAscii(oldFn), wxString::FromAscii(newFn));
	AtObj obj = AtlasObject::LoadFromXML(L"e:/0ad/svnc/trunk/binaries/data/mods/official/art/actors_clean/" + wxString::FromAscii(oldFn));
	wxASSERT(obj.defined());
	obj = ConvertToLatestFormat(obj);
//	wxString x = AtlasObject::ConvertToString(obj).c_str();
//	wxLogDebug(x);
	bool ok = AtlasObject::SaveToXML(obj, L"e:/0ad/svnc/trunk/binaries/data/mods/official/art/actors_new/" + wxString::FromAscii(newFn));
	wxASSERT(ok);
//*/
}

void convert_actors()
{
//	convert_file("fauna/deer1.xml", "fauna/temp_deer1.xml");
#include "e:/0ad/svnc/trunk/binaries/data/mods/official/art/convert.cpp"
}
#endif
