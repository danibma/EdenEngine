#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>

#include <UI/UI.h>
#include "Scene/SceneSerializer.h"

namespace Eden
{
	ContentBrowserPanel::ContentBrowserPanel(std::shared_ptr<IRHI>& rhi)
	{
		m_RHI = rhi;

		m_EditorIcons["File"] = rhi->CreateTexture("assets/editor/file.png");
		m_EditorIcons["Folder"] = rhi->CreateTexture("assets/editor/folder.png");
		m_EditorIcons["Back"] = rhi->CreateTexture("assets/editor/icon_back.png");

		m_EdenExtensions.emplace_back(SceneSerializer::DefaultExtension);
		m_EdenExtensions.emplace_back(".gltf");
		m_EdenExtensions.emplace_back(".glb");
		m_EdenExtensions.emplace_back(".hdr");
	}

	ContentBrowserPanel::~ContentBrowserPanel()
	{
	}

	void ContentBrowserPanel::DrawContentOutlinerFolder(std::filesystem::directory_entry directory)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

		auto path = directory.path().string();
		auto folder_name = directory.path().stem().string();

		ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

		if (m_CurrentPath == directory.path())
			base_flags |= ImGuiTreeNodeFlags_Selected;

		bool node_open = UI::TreeNodeWithIcon((ImTextureID)m_RHI->GetTextureID(m_EditorIcons["Folder"]), ImVec2(18, 18), folder_name.c_str(), base_flags);

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			m_CurrentPath = directory.path();

		if (node_open)
		{
			for (auto& p : std::filesystem::directory_iterator(directory))
			{
				if (p.is_directory())
					DrawContentOutlinerFolder(p);
			}

			ImGui::TreePop();
		}

		ImGui::PopStyleVar();
	}

	void ContentBrowserPanel::Search(const char* path)
	{
		for (auto& p : std::filesystem::directory_iterator(path))
		{
			DirectoryInfo info;
			info.filename = p.path().filename().string();
			info.extension = p.path().extension().string();
			info.path = p.path().string();
			info.is_directory = p.is_directory();

			if (strstr(info.filename.c_str(), m_SearchBuffer) != nullptr)
				DrawDirectory(info);

			if (info.is_directory)
				Search(info.path.c_str());
		}
	}

	bool ContentBrowserPanel::DrawDirectory(DirectoryInfo& info)
	{
		if (info.is_directory)
		{
			ImGui::PushID(info.filename.c_str());
			ImGui::TableNextColumn();
			ImGui::ImageButton((ImTextureID)m_RHI->GetTextureID(m_EditorIcons["Folder"]), ImVec2(m_ThumbnailSize, m_ThumbnailSize));
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
				m_CurrentPath = info.path;
		}
		else
		{
			bool valid_extension = false;
			for (auto& eden_extension : m_EdenExtensions)
			{
				if (info.extension == eden_extension)
					valid_extension = true;
			}

			if (valid_extension)
			{
				ImGui::PushID(info.filename.c_str());
				ImGui::TableNextColumn();
				ImGui::ImageButton((ImTextureID)m_RHI->GetTextureID(m_EditorIcons["File"]), ImVec2(m_ThumbnailSize, m_ThumbnailSize));
				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, info.path.c_str(), info.path.size() + 1);
					ImGui::Text(info.filename.c_str());
					ImGui::EndDragDropSource();
				}
			}
			else
			{
				return false;
			}
		}

		float middle_of_button = (m_ThumbnailSize + m_ThumbnailPadding) / 2;
		float middle_of_text = ImGui::CalcTextSize(info.filename.c_str()).x / 2;
		float button_padding = ImGui::GetStyle().FramePadding.x;

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + button_padding);
		ImGui::TextWrapped(info.filename.c_str());
		ImGui::PopID();

		return false;
	}

	void ContentBrowserPanel::Render()
	{
		if (!open_content_browser) return;

		ImGui::Begin("Content Browser", &open_content_browser, ImGuiWindowFlags_NoScrollbar);

		if (ImGui::BeginTable(CONTENT_BROWSER_COLUMN, 2, ImGuiTableFlags_Resizable, ImVec2(ImGui::GetContentRegionAvail())))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
			ImGui::BeginChild("##cb_outliner");
			if (ImGui::CollapsingHeader("Content", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick))
			{
				for (auto& p : std::filesystem::directory_iterator(s_AssetsDirectory))
				{
					if (p.is_directory())
					{
						DrawContentOutlinerFolder(p);
					}
				}
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(1);

			ImGui::BeginChild("##cb_items");
			// Back Button
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6));
			if (ImGui::ImageButton((ImTextureID)m_RHI->GetTextureID(m_EditorIcons["Back"]), ImVec2(20, 20), ImVec2(0, 0),
								   ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 0.7f)))
			{
				if (m_CurrentPath.string() != std::string(s_AssetsDirectory))
					m_CurrentPath = m_CurrentPath.parent_path();
			}
			ImGui::PopStyleVar();
			ImGui::SameLine();

			// Search
			ImGui::SetNextItemWidth(200.0f);
			ImGui::InputTextWithHint("###search", "Search:", m_SearchBuffer, sizeof(m_SearchBuffer));
			ImGui::Separator();

			bool use_search = strcmp(m_SearchBuffer, "");
			float cell_size = m_ThumbnailSize + m_ThumbnailPadding;
			int column_count = (int)(ImGui::GetContentRegionAvail().x / cell_size);
			if (ImGui::BeginTable(CONTENT_BROWSER_CONTENTS_COLUMN, column_count))
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				if (use_search)
				{
					Search(m_CurrentPath.string().c_str());
				}
				else
				{
					for (auto& p : std::filesystem::directory_iterator(m_CurrentPath))
					{
						DirectoryInfo info;
						info.filename = p.path().filename().string();
						info.extension = p.path().extension().string();
						info.path = p.path().string();
						info.is_directory = p.is_directory();

						if (!DrawDirectory(info))
							continue;
					}
				}
				ImGui::PopStyleColor();
				ImGui::EndTable();
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::EndTable();
		}

		ImGui::End();
	}
}