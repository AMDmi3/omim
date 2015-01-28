#pragma once

#include <jni.h>

#include "../../../../../drape/pointers.hpp"
#include "../../../../../drape/oglcontextfactory.hpp"

#include "../../../../../map/framework.hpp"

#include "../../../../../search/result.hpp"

#include "../../../../../platform/country_defines.hpp"

#include "../../../../../geometry/avg_vector.hpp"

#include "../../../../../base/deferred_task.hpp"
#include "../../../../../base/strings_bundle.hpp"
#include "../../../../../base/timer.hpp"

#include "../../../../../indexer/map_style.hpp"

#include "../../../../../std/shared_ptr.hpp"
#include "../../../../../std/map.hpp"

namespace android
{
  class Framework : public storage::CountryTree::CountryTreeListener,
                    public storage::ActiveMapsLayout::ActiveMapsListener
  {
  private:
    dp::MasterPointer<dp::ThreadSafeFactory> m_contextFactory;
    ::Framework m_work;

    typedef shared_ptr<jobject> TJobject;

    TJobject m_javaCountryListener;
    typedef map<int, TJobject> TListenerMap;
    TListenerMap m_javaActiveMapListeners;
    int m_currentSlotID;

    int m_activeMapsConnectionID;

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
    int m_mask;

    bool m_doLoadState;

    /// @name Single click processing parameters.
    //@{
    my::Timer m_doubleClickTimer;
    bool m_isCleanSingleClick;
    double m_lastX1;
    double m_lastY1;
    //@}

    math::LowPassVector<float, 3> m_sensors[2];
    double m_lastCompass;

    unique_ptr<DeferredTask> m_deferredTask;
    bool m_wasLongClick;

    int m_densityDpi;
    int m_screenWidth;
    int m_screenHeight;

    void StartTouchTask(double x, double y, unsigned ms);
    void KillTouchTask();
    void OnProcessTouchTask(double x, double y, unsigned ms);

    string m_searchQuery;

    float GetBestDensity(int densityDpi);

    bool InitRenderPolicyImpl(int densityDpi, int screenWidth, int screenHeight);

  public:
    Framework();
    ~Framework();

    storage::Storage & Storage();

    void DontLoadState() { m_doLoadState = false; }

    void ShowCountry(storage::TIndex const & idx, bool zoomToDownloadButton);
    storage::TStatus GetCountryStatus(storage::TIndex const & idx) const;

    void OnLocationError(int/* == location::TLocationStatus*/ newStatus);
    void OnLocationUpdated(location::GpsInfo const & info);
    void OnCompassUpdated(location::CompassInfo const & info);
    void UpdateCompassSensor(int ind, float * arr);

    void Invalidate();

    bool CreateDrapeEngine(JNIEnv * env, jobject jSurface, int densityDpi);
    void DeleteDrapeEngine();

    void SetMapStyle(MapStyle mapStyle);

    void Resize(int w, int h);

    void Move(int mode, double x, double y);
    void Zoom(int mode, double x1, double y1, double x2, double y2);
    void Touch(int action, int mask, double x1, double y1, double x2, double y2);

    /// Show rect from another activity. Ensure that no LoadState will be called,
    /// when main map activity will become active.
    void ShowSearchResult(search::Result const & r);
    void ShowAllSearchResults();

    bool Search(search::SearchParams const & params);
    string GetLastSearchQuery() { return m_searchQuery; }
    void ClearLastSearchQuery() { m_searchQuery.clear(); }

    void LoadState();
    void SaveState();

    void SetupMeasurementSystem();

    void AddLocalMaps();
    void RemoveLocalMaps();

    void GetMapsWithoutSearch(vector<string> & out) const;

    storage::TIndex GetCountryIndex(double lat, double lon) const;
    string GetCountryCode(double lat, double lon) const;

    string GetCountryNameIfAbsent(m2::PointD const & pt) const;
    m2::PointD GetViewportCenter() const;

    void AddString(string const & name, string const & value);

    void Scale(double k);

    BookmarkAndCategory AddBookmark(size_t category, m2::PointD const & pt, BookmarkData & bm);
    void ReplaceBookmark(BookmarkAndCategory const & ind, BookmarkData & bm);
    size_t ChangeBookmarkCategory(BookmarkAndCategory const & ind, size_t newCat);

    ::Framework * NativeFramework();
    PinClickManager & GetPinClickManager() { return m_work.GetBalloonManager(); }

    bool IsDownloadingActive();

    bool ShowMapForURL(string const & url);

    void DeactivatePopup();

    string GetOutdatedCountriesString();

    void ShowTrack(int category, int track);

    void SetCountryTreeListener(shared_ptr<jobject> objPtr);
    void ResetCountryTreeListener();

    int AddActiveMapsListener(shared_ptr<jobject> obj);
    void RemoveActiveMapsListener(int slotID);

    // Fills mapobject's metadata from UserMark
    void InjectMetadata(JNIEnv * env, jclass clazz, jobject const mapObject, UserMark const * userMark);

  public:
    virtual void ItemStatusChanged(int childPosition);
    virtual void ItemProgressChanged(int childPosition, storage::LocalAndRemoteSizeT const & sizes);

    virtual void CountryGroupChanged(storage::ActiveMapsLayout::TGroup const & oldGroup, int oldPosition,
                                     storage::ActiveMapsLayout::TGroup const & newGroup, int newPosition);
    virtual void CountryStatusChanged(storage::ActiveMapsLayout::TGroup const & group, int position,
                                      storage::TStatus const & oldStatus, storage::TStatus const & newStatus);
    virtual void CountryOptionsChanged(storage::ActiveMapsLayout::TGroup const & group,
                                       int position, TMapOptions const & oldOpt,
                                       TMapOptions const & newOpt);
    virtual void DownloadingProgressUpdate(storage::ActiveMapsLayout::TGroup const & group, int position,
                                           storage::LocalAndRemoteSizeT const & progress);
  };
}

extern android::Framework * g_framework;
