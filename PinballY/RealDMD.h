// This file is part of PinballY
// Copyright 2018 Michael J Roberts | GPL v3 or later | NO WARRANTY
//
// Real DMD interface

#pragma once
#include "VLCAudioVideoPlayer.h"
#include "DmdDeviceDll.h"

class ErrorHandler;
class GameListItem;
class VLCAudioVideoPlayer;
class GameListItem;
class GameSystem;

class RealDMD : public VLCAudioVideoPlayer::DMD
{
public:
	RealDMD();
	~RealDMD();

	// Locate the DLL.  Returns true if the DLL is found, false if not
	bool FindDLL();

	// initialize: load the device interface DLL and open the device
	bool Init(ErrorHandler &eh);

	// shut down
	void Shutdown();

	// Update the display to match the current game list selection
	void UpdateGame();

	// Clear media.  This removes any playing video and clears the
	// last game record.  This should be called before launching a
	// game program, to make sure we're not trying to send updates
	// to the display while the game might be using it.
	void ClearMedia(bool forgetGame = true);

	// apply a working audio level to playing media
	void ApplyWorkingAudioVolume(int volPct);

	// update the global video muting
	void OnUpdateVideoMute(bool mute);

	// Enter/exit running game mode.  If possible, we'll close our
	// session with the DMD while a game is running, to avoid any
	// contention with the game over access to the physical device.
	void BeginRunningGameMode(GameListItem *game, GameSystem *system);
	void EndRunningGameMode();

	// Do we continue showing media during the given game launch?
	bool ShowMediaWhenRunning(GameListItem *game, GameSystem *system);

	// Receive notification that new high scores have been received
	// for a given game.
	void OnUpdateHighScores(GameListItem *game);

	// End of presentation notification.  The main window calls this
	// when it gets an AVPEndOfPresentation message from the video
	// player.
	void VideoEndOfPresentation(WPARAM cookie);

	// Loop Needed notification.  The main window calls this when it
	// gets an AVPMsgLoopNeeded message from the video player.  If
	// the cookie matches our current video's cookie, we'll replay
	// our video.
	void VideoLoopNeeded(WPARAM cookie);

	// 
	// video player callback interface
	// 

	// present a frame
	virtual void PresentVideoFrame(int width, int height,
		const BYTE *y, const BYTE *u, const BYTE *v) override;

	// do we support RGB?
	virtual bool SupportsRGBDisplay() const override;

	// get/set mirroring
	bool IsMirrorHorz() const { return mirrorHorz; }
	bool IsMirrorVert() const { return mirrorVert; }
	void SetMirrorHorz(bool b);
	void SetMirrorVert(bool b);

	// startup video support
	bool LoadStartupVideo();
	bool PlayStartupVideo();
	void EndStartupVideo();
	void FadeStartupVideo(float amount);
	bool IsStartupVideoPlaying() const;

	// initialize gamma from the settings
	void UpdateGamma();

protected:
	// singleton instance
	static RealDMD *inst;

	// Test runtime conditions to determine if the DMD should be
	// enabled.
	static bool ShouldEnable();

	// open the session with the DLL
	void OpenSession();

	// close the session with the DLL
	void CloseSession();

	// set the game settings in the DLL
	void SetGameSettings(const char *gameName, const DMDDevice::tPMoptions &opts);

	// Is the DMD enabled?  We might disable the DMD even if we
	// successfully loaded the DLL, such as when dmd-extensions is
	// used and it's in virtual mode.
	bool enabled;

	// Is the session open?
	bool sessionOpen = false;

	// Color space for a stored image.  This specifies the type of
	// pixel data stored in the image, and the render function we 
	// use to display it.
	enum ColorSpace
	{
		DMD_COLOR_MONO4,		// 4-shade grayscale
		DMD_COLOR_MONO16,		// 16-shade grayscale
		DMD_COLOR_RGB			// 24-bit RGB
	};

	// gamma value
	float gamma;

	// Precomputed monochrome gamma table.  This is the map from
	// a linear grayscale luminance value to a gamma-corrected
	// luminance value.
	BYTE gammaMap[256];

	// default PM_GameSettings options
	static const DMDDevice::tPMoptions defaultOpts;

	// Device writer thread.  Some device-specific dmddevice.dll
	// implementations might block on the data transfer to the 
	// physical device (e.g., a USB write).  We don't want the
	// UI thread to get blocked on these writes, so we use a
	// separate thread.
	HandleHolder hWriterThread;

	// thread exit flag
	volatile bool writerThreadQuit;

	// writer thread entrypoint
	static DWORD WINAPI SWriterThreadMain(LPVOID lParam)
		{ return reinterpret_cast<RealDMD*>(lParam)->WriterThreadMain(); }
	DWORD WriterThreadMain();

	// Device writer event.  We signal this event whenever we
	// add a frame to the write queue.
	HandleHolder hWriterEvent;

	// Write queue lock
	CriticalSection writeFrameLock;

	// DMD DLL access lock
	static CriticalSection dmdLock;

	// load the DLL
	bool LoadDLL(ErrorHandler &eh);

	// DLL location - set by FindDLL()
	static TSTRING dllPath;

	// DLL module handle
	static HMODULE hmodDll;

	// Have we attempted to load the DLL yet?
	static bool dllLoaded;

	// is the DLL valid?
	static bool IsDllValid() { return hmodDll != NULL; }

	// Information on the dmd-extensions version of dmddevice.dll
	static struct DmdExtInfo
	{
		DmdExtInfo() : 
			matched(false), 
			virtualEnabled(true),
			settingsFix(false),
			virtualCloseFix(false)
		{ }

		// Is this the dmd-extensions version of dmddevice.dll?
		bool matched;

		// Is the virtual DMD enabled?
		bool virtualEnabled;

		// Does this version of the DLL have the PM_GameSettings()
		// fix?  Older versions of the DLL crashed if we called
		// PM_GameSettings() more than once per process lifetime.
		// Releases from 1.7.3 have a fix.
		bool settingsFix;

		// Does this version of the DLL have the Close()/Open()
		// fix?  Older versions of the DLL crashed if we called
		// Close() and then called Open() again within the
		// process lifetime.  Releases from 1.7.3 have a fix.
		bool virtualCloseFix;

	} dmdExtInfo;

	// vertical/horizontal mirroring
	bool mirrorHorz;
	bool mirrorVert;

	// Monochrome base color for the current game, from the 
	// VPinMAME settings for the game's ROM.
	COLORREF baseColor;

	// current game selection
	GameListItem *curGame;

	// Force a reload of the current game's media
	void ReloadGame();

	// video player
	RefPtr<VLCAudioVideoPlayer> videoPlayer;

	// color space for the video
	ColorSpace videoColorSpace;

	// video mode
	enum VideoMode
	{
		None,      // no video playing
		Game,      // game video
		Startup    // startup video
	};
	VideoMode videoMode = None;

	// load a video
	bool LoadVideo(const TCHAR *path, bool play, bool looping, int volPct, bool mute, VideoMode mode, ErrorHandler &eh);

	// Set the oclor scheme for a game
	void SetColorScheme(GameListItem *game);

	// Generate high score graphics for the current game
	void GenerateHighScoreGraphics();

	// The "slide show".  This is a series of still images that
	// we display on the DMD.  If there's a video, we display
	// these in alternation with the video.
	//
	// If there's a still image for the game (from the "Real DMD
	// Images" media folder), it's the first slide in the list.
	// Note that we only use one media type for each game - still
	// image or video, not both
	// 
	// If we have high score graphics, they appear in this list.
	// High score graphics (unlike the still image from the media
	// folder) can be used in combination with a video. 
	struct Slide : RefCounted
	{
		// Slide type
		enum SlideType
		{
			EmptySlide,      // generated empty image
			MediaSlide,      // still image from the game's media folder
			HighScoreSlide   // generated high score screen
		} slideType;

		Slide(ColorSpace colorSpace, BYTE *pix, DWORD displayTime, SlideType slideType) :
			colorSpace(colorSpace),
			pix(pix),
			displayTime(displayTime),
			slideType(slideType)
		{
		}

		// The image's color type - this selects the device DLL
		// function that we use to display it
		ColorSpace colorSpace;

		// Pixel array for the image
		std::unique_ptr<BYTE> pix;

		// display time for this image, in milliseconds
		DWORD displayTime;
	};
	std::list<RefPtr<Slide>> slideShow;

	// Empty screen slide.  We use this to clear the display when
	// appropriate.
	RefPtr<Slide> emptySlide;

	// Next frame to write.  Note that we only keep a single frame
	// buffered, since the main thread produces frames for display
	// in real time.  There's no point in keeping missed frames;
	// we'll just drop any frame that we don't manage to send to
	// the device before the next frame arrives.
	RefPtr<Slide> writerFrame;

	// Game settings, to send via the writer thread.  The DLL export
	// can be quite slow in some implementations (about 250ms in
	// dmd-extensions), so, as with the video frames, we want to
	// make these calls from our background thread to avoid blocking
	// the UI.
	struct GameSettings
	{
		GameSettings(const char *gameName, const DMDDevice::tPMoptions &opts) :
			gameName(gameName), opts(opts) { }
		CSTRING gameName;
		DMDDevice::tPMoptions opts;
	};
	std::unique_ptr<GameSettings> writerSettings;

	// send a frame to the writer
	void SendWriterFrame(Slide *slide);

	// start slide show playback
	void StartSlideShow();

	// advance to the next slide
	void NextSlide();

	// render the current slide
	void RenderSlide();

	// current slide show position
	decltype(slideShow)::const_iterator slideShowPos;

	// slide show timer ID
	UINT_PTR slideShowTimerID;

	// set the slide show timer
	void SetSlideShowTimer();

	// is the slide show timer running?
	bool slideShowTimerRunning = false;

	// timer event handler
	static VOID CALLBACK SlideTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

	// log a DMD-related event
	static void LogGroup();
	static void Log(const TCHAR *msg, ...);
};
