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
		bool bIsDirectory;
		std::vector<DirectoryInfo> subDirectories;
	};

	struct Texture;
	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel();
		void Render();

	private:
		void DrawContentOutlinerFolder(std::filesystem::directory_entry directory);
		void Search(const char* path);
		bool DrawDirectory(DirectoryInfo& info);

	private:
		std::filesystem::path m_CurrentPath = s_AssetsDirectory;
		char m_SearchBuffer[32] = "\0";
		float m_ThumbnailPadding = 16.0f;
		float m_ThumbnailSize = 91.0f;

	public:
		bool bOpenContentBrowser = true;
	};
}

