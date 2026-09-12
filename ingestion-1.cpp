#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
using namespace std;

// Dynamically-sized array of records (vector) — chosen over a fixed-size
// raw array since we don't hardcode a max row count; still array-backed
// (contiguous memory, index access) rather than a linked structure.
struct TurbineRecord {
    string logTime;
    double windspeed;
    double rpm;
    double voltageIn;
    double voltageL1;
    double voltageL2;
    double currentOut;
    double powerOut;
    double powerReg;
    double t1;
    double t2;
    double t3;
    int eventCount;
    int lastEventCode;
    int turbineStatus;
    int gridStatus;
    int systemStatus;
};

bool hasBit(int status, int bit) {
    return (status & bit) != 0;
}

bool isValid(TurbineRecord rec) {
    if (rec.windspeed < 0 || rec.windspeed > 25) {
        return false;
    }
    if (rec.rpm < 0) {
        return false;
    }
    if (rec.powerOut < 0) {
        return false;
    }
    if (rec.t1 < -10 || rec.t1 > 60) {
        return false;
    }
    return true;
}

bool checkWindPowerMismatch(TurbineRecord rec) {
    // Bit 1 = Low Windspeed, Bit 2 = Braking (see status bit reference).
    // Using hasBit() instead of an enumerated list of status values means
    // this correctly catches ANY combo with either bit set (e.g. 9, 41),
    // not just the specific combos seen in one dataset sample.
    bool alreadyIdle = hasBit(rec.turbineStatus, 1) || hasBit(rec.turbineStatus, 2);
    if (rec.windspeed >= 4 && rec.powerOut < 50 && !alreadyIdle) {
        return true; // fault detected
    }
    return false;
}

bool checkTempWithoutLoad(TurbineRecord rec) {
    // Flags the inverter heating up (T1) relative to ambient (T3) while
    // producing little/no power — normal delta is ~2.1-2.7C, so >5C
    // indicates a genuine deviation rather than ordinary ambient heat.
    double delta = rec.t1 - rec.t3;
    if (rec.powerOut < 10 && delta > 5) {
        return true; // fault detected
    }
    return false;
}

bool checkTipSpeedRatioAnomaly(TurbineRecord rec) {
    // Ratio is only meaningful once there's real wind; below 3 m/s the
    // ratio is naturally noisy due to low-speed measurement resolution.
    if (rec.windspeed < 3) {
        return false;
    }
    double ratio = rec.rpm / rec.windspeed;
    if (ratio < 20 || ratio > 55) {
        return true; // fault detected
    }
    return false;
}

bool checkRapidChange(TurbineRecord prev, TurbineRecord curr) {
    // Needs the PREVIOUS reading, unlike the other three rules which only
    // look at one row — compares Power out between two consecutive
    // 1-minute readings.
    double diff = curr.powerOut - prev.powerOut;
    if (diff < 0) {
        diff = -diff; // abs value
    }
    if (diff > 500) {
        return true; // fault detected
    }
    return false;
}

bool tryStod(const string& s, double& out) {
    try { out = stod(s); return true; }
    catch (...) { return false; }
}

bool tryStoi(const string& s, int& out) {
    try { out = stoi(s); return true; }
    catch (...) { return false; }
}

int main() {
    string filename;
    cout << "Enter dataset filename: ";
    cin >> filename;

    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error... Could not open file" << endl;
        return 1;
    }

    string line;
    getline(file, line); // skip header

    int skippedCount = 0;
    vector<TurbineRecord> records;

    while (getline(file, line)) {
        stringstream ss(line);
        string field;
        vector<string> row;

        while (getline(ss, field, ';')) {
            row.push_back(field);
        }

        if (row.size() < 17) {
            skippedCount++;
            continue;
        }

        TurbineRecord rec;
        rec.logTime = row[0];

        bool ok = true;
        ok = ok && tryStod(row[1], rec.windspeed);
        ok = ok && tryStod(row[2], rec.rpm);
        ok = ok && tryStod(row[3], rec.voltageIn);
        ok = ok && tryStod(row[4], rec.voltageL1);
        ok = ok && tryStod(row[5], rec.voltageL2);
        ok = ok && tryStod(row[6], rec.currentOut);
        ok = ok && tryStod(row[7], rec.powerOut);
        ok = ok && tryStod(row[8], rec.powerReg);
        ok = ok && tryStod(row[9], rec.t1);
        ok = ok && tryStod(row[10], rec.t2);
        ok = ok && tryStod(row[11], rec.t3);
        ok = ok && tryStoi(row[12], rec.eventCount);
        ok = ok && tryStoi(row[13], rec.lastEventCode);
        ok = ok && tryStoi(row[14], rec.turbineStatus);
        ok = ok && tryStoi(row[15], rec.gridStatus);
        ok = ok && tryStoi(row[16], rec.systemStatus);

        if (!ok) {
            skippedCount++;
            continue;
        }

        if (isValid(rec)) {
            records.push_back(rec);
        } else {
            skippedCount++;
        }
    }

    file.close();
    cout << "Loaded " << records.size() << " records, skipped " << skippedCount << " rows." << endl;
    return 0;
}
