#pragma once

#include <filesystem>
#include <unordered_map>

#define CONTENT_BROWSER_COLUMN "CONTENT_BROWSER_COLUMN"
#define CONTENT_BROWSER_CONTENTS_COLUMN "CONTENT_BROWSER_CONTENTS_COLUMN"
#define CONTENT_BROWSER_DRAG_DROP "CONTENT_BROWSER_DRAG_DROP"

namespace Eden
{
	constexpr char* s_AssetsDirectory = "assets";

	struct DirectoryInfo
	{
		std::string filename;
		std::string extension;
		std::string path;
		bool is_directory;
		std::vector<DirectoryInfo> sub_directories;
	};

	class IRHI;
	struct Texture;
	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel() = default;
		ContentBrowserPanel(std::shared_ptr<IRHI>& rhi);
		~ContentBrowserPanel();
		void Render();

	private:
		void DrawContentOutlinerFolder(std::filesystem::directory_entry directory);
		void Search(const char* path);
		bool DrawDirectory(DirectoryInfo& info);

	private:
		std::unordered_map<const char*, std::shared_ptr<Texture>> m_EditorIcons;
		std::filesystem::path m_CurrentPath = s_AssetsDirectory;
		std::vector<std::string> m_EdenExtensions;
		char m_SearchBuffer[32] = "\0";
		float m_ThumbnailPadding = 16.0f;
		float m_ThumbnailSize = 91.0f;
		std::shared_ptr<IRHI> m_RHI;

	public:
		bool open_content_browser = true;
	};
}

