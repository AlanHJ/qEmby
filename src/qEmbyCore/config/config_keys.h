#ifndef CONFIG_KEYS_H
#define CONFIG_KEYS_H

#include <QString>

namespace ConfigKeys {


inline QString forServer(const QString& serverId, const char* baseKey) {
    return QStringLiteral("server/%1/%2").arg(serverId, baseKey);
}


inline QString forLibrary(const QString& serverId, const QString& libraryId, const char* baseKey) {
    return QStringLiteral("server/%1/library/%2/%3").arg(serverId, libraryId, baseKey);
}



constexpr const char* Language = "general/language";
constexpr const char* RememberServer = "general/remember_server";
constexpr const char* LastSelectedServerId = "general/last_selected_server_id";
constexpr const char* CloseToTray = "general/close_to_tray";
constexpr const char* LogEnable = "general/log_enable";
constexpr const char* ApiTimeout = "general/api_timeout";
constexpr const char* ImageCacheLimit = "general/image_cache_limit";




constexpr const char* ThemeMode = "appearance/theme_mode";
constexpr const char* DefaultLibraryView = "appearance/default_library_view";
constexpr const char* FontSize = "appearance/font_size";
constexpr const char* SidebarPosition = "appearance/sidebar_position";
constexpr const char* SidebarPinned = "appearance/sidebar_pinned";
constexpr const char* StartupWindowState = "appearance/startup_window_state";
constexpr const char* UiAnimations = "appearance/ui_animations";
constexpr const char* SnapshotNavigation = "appearance/snapshot_navigation";
constexpr const char* SearchHistoryEnabled = "appearance/search_history_enabled";
constexpr const char* SearchAutocompleteEnabled = "appearance/search_autocomplete_enabled";
constexpr const char* ShowRecommended = "library/show_recommended";
constexpr const char* ShowContinueWatching = "library/show_continue_watching";
constexpr const char* ShowLatestAdded = "library/show_latest_added";
constexpr const char* ShowMediaLibraries = "library/show_media_libraries";
constexpr const char* ShowEachLibrary = "library/show_each_library";
constexpr const char* ShowFavoriteFolders = "library/show_favorite_folders";
constexpr const char* CustomHomeSectionOrderEnabled =
    "library/custom_home_section_order_enabled";
constexpr const char* HomeSectionOrder = "library/home_section_order";
constexpr const char* ImageQuality = "library/image_quality";
constexpr const char* ShimmerAnimation = "library/shimmer_animation";
constexpr const char* AdaptiveImages = "library/adaptive_images";


constexpr const char* LibrarySortIndex = "sort/index";
constexpr const char* LibrarySortDescending = "sort/descending";




constexpr const char* DataCacheDuration = "cache/data_cache_duration";
constexpr const char* ImageCacheDuration = "cache/image_cache_duration";




constexpr const char* PlayerHwDec = "player/hw_decoding";
constexpr const char* PlayerVo = "player/video_output";
constexpr const char* PlayerVideoSync = "player/video_sync";
constexpr const char* PlayerDefaultScale = "player/default_scale";
constexpr const char* PlayerAudioLang = "player/audio_lang";
constexpr const char* PlayerSubLang = "player/sub_lang";
constexpr const char* PlayerPreferredVersion = "player/preferred_version";
constexpr const char* PlayerVolNormal = "player/vol_normalization";
constexpr const char* PlayerSeekStep = "player/seek_step";
constexpr const char* PlayerLongPressSeek = "player/long_press_seek";
constexpr const char* PlayerLongPressMode = "player/long_press_mode";
constexpr const char* PlayerLongPressTriggerMs = "player/long_press_trigger_ms";
constexpr const char* PlayerMouseEdgeLongPress =
    "player/mouse_edge_long_press";
constexpr const char* PlayerMediaSwitcherMode =
    "player/media_switcher_mode";
constexpr const char* PlayerClickToPause = "player/click_to_pause";
constexpr const char* PlayerIndependentWindow = "player/independent_window";
constexpr const char* PlayerUseMpvConf = "player/use_mpv_conf";
constexpr const char* PlayerVolumeLevel = "player/volume_level";
constexpr const char* PlayerVolumeMuted = "player/volume_muted";
constexpr const char* PlayerSubtitleFont = "player/subtitle_font";
constexpr const char* PlayerSubtitleDelayMs = "player/subtitle_delay_ms";
constexpr const char* PlayerSubtitleFontSize = "player/subtitle_font_size";
constexpr const char* PlayerSubtitlePosition = "player/subtitle_position";
constexpr const char* PlayerSubtitleOutlineSize =
    "player/subtitle_outline_size";
constexpr const char* PlayerSubtitleShadowOffset =
    "player/subtitle_shadow_offset";
constexpr const char* PlayerSubtitleScale = "player/subtitle_scale";
constexpr const char* PlayerDanmakuEnabled = "player/danmaku_enabled";
constexpr const char* PlayerDanmakuOpacity = "player/danmaku_opacity";
constexpr const char* PlayerDanmakuFontScale = "player/danmaku_font_scale";
constexpr const char* PlayerDanmakuAreaPercent = "player/danmaku_area_percent";
constexpr const char* PlayerDanmakuDensity = "player/danmaku_density";
constexpr const char* PlayerDanmakuSpeedScale = "player/danmaku_speed_scale";
constexpr const char* PlayerDanmakuOffsetMs = "player/danmaku_offset_ms";
constexpr const char* PlayerDanmakuHideScroll = "player/danmaku_hide_scroll";
constexpr const char* PlayerDanmakuHideTop = "player/danmaku_hide_top";
constexpr const char* PlayerDanmakuHideBottom = "player/danmaku_hide_bottom";
constexpr const char* PlayerDanmakuBlockedKeywords =
    "player/danmaku_blocked_keywords";
constexpr const char* PlayerDanmakuDualSubtitle =
    "player/danmaku_dual_subtitle";




constexpr const char* DanmakuProvider = "danmaku/provider";
constexpr const char* DanmakuProviderBaseUrl = "danmaku/provider_base_url";
constexpr const char* DanmakuProviderAppId = "danmaku/provider_app_id";
constexpr const char* DanmakuProviderAppSecret = "danmaku/provider_app_secret";
constexpr const char* DanmakuServers = "danmaku/servers";
constexpr const char* DanmakuSelectedServer = "danmaku/selected_server";
constexpr const char* DanmakuSourceMode = "danmaku/source_mode";
constexpr const char* DanmakuAutoLoad = "danmaku/auto_load";
constexpr const char* DanmakuAutoMatch = "danmaku/auto_match";
constexpr const char* DanmakuWithRelated = "danmaku/with_related";
constexpr const char* DanmakuCacheHours = "danmaku/cache_hours";




constexpr const char* ExtPlayerEnable = "ext_player/enable";
constexpr const char* ExtPlayerPath = "ext_player/path";
constexpr const char* ExtPlayerArgs = "ext_player/arguments";
constexpr const char* ExtPlayerDirectStream = "ext_player/direct_stream";
constexpr const char* ExtPlayerQuickPlay = "ext_player/quick_play";
constexpr const char* ExtPlayerUrlReplace = "ext_player/url_replace";
constexpr const char* ExtPlayerDetectedList = "ext_player/detected_list"; 




constexpr const char* SearchHistoryRecords = "search/history_records"; 




constexpr const char* DownloadDirectory = "download/directory";
constexpr const char* DownloadHistoryRecords = "download/history_records";
constexpr const char* DownloadDeleteFileWithRecord =
    "download/delete_file_with_record";
}

#endif 
