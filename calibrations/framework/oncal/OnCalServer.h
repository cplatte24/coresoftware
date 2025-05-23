#ifndef ONCAL_ONCALSERVER_H
#define ONCAL_ONCALSERVER_H

#include <fun4all/Fun4AllServer.h>
#include <phool/PHTimeStamp.h>

#include <time.h>  // for time_t
#include <map>
#include <set>
#include <string>
#include <vector>

class OnCal;
class SubsysReco;
class TH1;

namespace fetchrun
{
  enum
  {
    CLOSEST,
    PREVIOUS
  };
};

class OnCalServer : public Fun4AllServer
{
 public:
  static OnCalServer *instance();
  ~OnCalServer() override;
  using Fun4AllServer::registerHisto;
  void registerHisto(TH1 *h1d, OnCal *Calibrator, const int replace = 0);
  void unregisterHisto(const std::string &calibratorname);
  void Print(const std::string &what = "ALL") const override;

  void dumpHistos();
  int process_event() override;
  int BeginRun(const int runno) override;
  int EndRun(const int /*runno*/) override { return 0; }  // do not execute EndRun
  int End() override;

  PHTimeStamp *GetEndValidityTS();

  PHTimeStamp *GetBeginValidityTS();
  void printStamps();
  PHTimeStamp *GetLastGoodRunTS(OnCal *calibrator, const int irun);

  void recordDataBase(const bool bookkeep = false);

  // RunNumber() tells the server which run is being analyzed.
  // and if recordDB is true, this will insert the run number in
  // calprocess_stat table in calBookKeep database.
  // All updates are made to the row in the database containing this runNum.
  // Note that the run number is the primary key in the tables.
  // If calBookKeep database is not to be updated, this function
  // should not be called.
  void RunNumber(const int runnum);
  int RunNumber() const { return runNum; }

  void BeginTimeStamp(const PHTimeStamp &TimeStp);
  void EndTimeStamp(const PHTimeStamp &TimeStp);

  int SyncCalibTimeStampsToOnCal(const OnCal *calibrator, const std::string &table, const int commit = 0);
  int SyncCalibTimeStampsToOnCal(const OnCal *calibrator, const int commit = 0);
  int SyncOncalTimeStampsToRunDB(const int commit = 0);
  int ClosestGoodRun(OnCal *calibrator, const int irun, const int previous = fetchrun::CLOSEST);
  static int CopyTables(const OnCal *calibrator, const int FromRun, const int ToRun, const int commit = 0);
  static int OverwriteCalibration(OnCal *calibrator, const int runno, const int commit = 0, const int fromrun = -1);
  int FixMissingCalibration(OnCal *calibrator, const int runno, const int commit = 0, const int fromrun = -1);

  int SetBorTime(const int runno);
  int SetEorTime(const int runno);
  int requiredCalibration(SubsysReco *reco, const std::string &calibratorname);
  int FindClosestCalibratedRun(const int irun);
  int FillRunListFromFileList();
  int AdjustRichTimeStampForMultipleRuns();
  int CreateCalibration(OnCal *calibrator, const int myrunnumber, const std::string &what, const int commit = 0);
  int GetCalibStatus(const std::string &calibname, const int runno);
  static int DisconnectDB();
  void TestMode(const int i = 1);
  // need to be able to call this from the outside
  bool updateDBRunRange(const std::string &table, const std::string &column, const int entry, const int firstrun, const int lastrun);
  void EventCheckFrequency(const unsigned int i) { eventcheckfrequency = i; }

 protected:
  //-------------------------------------
  // following functions access DB using odbc++ library
  // these are designed to insert status in calBookKeep (or success) database.
  // setDB() sets the name of the database to connect to. e.g., calibration
  // this database should exist in the odbc.ini file.
  // void setDB(const char* DBname){database = DBname;}
  bool connectDB();

  // insertRunNumInDB enters the run number in the calBookKeep database.
  // All other updates are made to rows in the database containing the runNum.
  // This function should be called before any updates are made.
  // Returns true on successful DB insert.
  bool insertRunNumInDB(const std::string &DBtable, const int runno);

  bool findRunNumInDB(const std::string &DBtable, const int runno);

  // these functions update different columns in the success database tables.
  // Ony the row with the run number set by setRunNum() is updated.

  bool updateDB(const std::string &table, const std::string &column, int entry);
  bool updateDB(const std::string &table, const std::string &column, bool entry);
  bool updateDB(const std::string &table, const std::string &column, const std::string &entry,
                const int runno, const bool append = false);
  int updateDB(const std::string &table, const std::string &column, const time_t ticks);

  int check_create_subsystable(const std::string &tablename);
  int check_create_successtable(const std::string &tablename);
  int add_calibrator_to_statustable(const std::string &calibratorname);
  int check_calibrator_in_statustable(const std::string &calibratorname);
  static int GetRunTimeTicks(const int runno, time_t &borticks, time_t &eorticks);
  void CreateCalibrationUpdateStatus(OnCal *calibrator, const std::string &table, const std::string &tablecomment, const int dbcode);
  OnCalServer(const std::string &name = "OnCalServer");
  PHTimeStamp beginTimeStamp;  // begin run timestamp of run analysing
  PHTimeStamp endTimeStamp;    // end run timestamp of run analysing
  int testmode{0};
  bool recordDB{false};
  TH1 *OnCalServerVars{nullptr};
  std::map<std::string, TH1 *> Histo;
  std::map<std::string, std::set<std::string> > calibratorhistomap;
  bool SetEndTimeStampByHand{false};
  bool SetBeginTimeStampByHand{false};

  std::string successTable;
  unsigned int runNum{0};
  unsigned int nEvents{0};
  unsigned int eventcheckfrequency{1000};
  std::string database{"calBookKeep"};  // this holds the name of the database
                                        // should be set to calibrations for normal running
  std::map<std::string, std::set<SubsysReco *> > requiredCalibrators;
  std::vector<int> analysed_runs;
  std::vector<std::string> inputfilelist;
  std::set<int> runlist;
};

#endif /* __ONCALSERVER_H */
