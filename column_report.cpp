#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <algorithm>
using namespace std;

// ===================== DATA =====================

struct TurbineRecord {
    string logTime;
    double windspeed, rpm, voltageIn, voltageL1, voltageL2;
    double currentOut, powerOut, powerReg, t1, t2, t3;
    int eventCount, lastEventCode;
    int turbineStatus, gridStatus, systemStatus;
};

const string columnNames[17] = {
    "Log Time", "Windspeed", "RPM", "Voltage In", "Voltage L1",
    "Voltage L2", "Current out", "Power out", "Power reg",
    "T1", "T2", "T3", "Event count", "Last event code",
    "Turbine status", "Grid status", "System status"
};

enum ColumnType { NUMERIC, COUNTER, BITMASK, TIMESTAMP };

// Every column type now offers exactly 4 operations (Timestamp's Search
// covers both "whole day" and "closest date+time" in one option), so the
// menu's Back/Change/Main positions never need to shift by type.
enum OperationType {
    RANGE_FILTER, THRESHOLD_FILTER, EXACT_FILTER, BIT_FILTER,
    DATE_FILTER, TIME_FILTER, SORT_OPERATION,
    NEAREST_SEARCH, EXACT_SEARCH, TIMESTAMP_SEARCH
};

struct Operation {
    OperationType type;
    int column;
    double value1 = 0, value2 = 0;
    bool firstGreater = true;
    bool ascending = true;
    string text1, text2;
};

// ===================== DATA LOADING =====================

bool toDouble(const string& text, double& value) {
    try { value = stod(text); return true; }
    catch (...) { return false; }
}

bool toInt(const string& text, int& value) {
    try { value = stoi(text); return true; }
    catch (...) { return false; }
}

bool hasBit(int status, int bit) {
    return (status & bit) != 0;
}

bool isValid(const TurbineRecord& r) {
    return r.windspeed >= 0 && r.windspeed <= 25 &&
           r.rpm >= 0 && r.powerOut >= 0 &&
           r.t1 >= -10 && r.t1 <= 60;
}

vector<TurbineRecord> loadData(const string& filename, int& skipped) {
    vector<TurbineRecord> data;
    skipped = 0;

    ifstream file(filename);
    if (!file) {
        cout << "Error: Could not open file.\n";
        return data;
    }

    string line;
    getline(file, line); // header

    while (getline(file, line)) {
        stringstream ss(line);
        string field;
        vector<string> row;

        while (getline(ss, field, ';'))
            row.push_back(field);

        if (row.size() < 17) {
            skipped++;
            continue;
        }

        TurbineRecord r;
        r.logTime = row[0];
        bool ok =
            toDouble(row[1], r.windspeed) &&
            toDouble(row[2], r.rpm) &&
            toDouble(row[3], r.voltageIn) &&
            toDouble(row[4], r.voltageL1) &&
            toDouble(row[5], r.voltageL2) &&
            toDouble(row[6], r.currentOut) &&
            toDouble(row[7], r.powerOut) &&
            toDouble(row[8], r.powerReg) &&
            toDouble(row[9], r.t1) &&
            toDouble(row[10], r.t2) &&
            toDouble(row[11], r.t3) &&
            toInt(row[12], r.eventCount) &&
            toInt(row[13], r.lastEventCode) &&
            toInt(row[14], r.turbineStatus) &&
            toInt(row[15], r.gridStatus) &&
            toInt(row[16], r.systemStatus);

        if (!ok || !isValid(r)) {
            skipped++;
            continue;
        }
        data.push_back(r);
    }

    return data;
}

// ===================== COLUMN ACCESS =====================

ColumnType getColumnType(int c) {
    if (c == 0) return TIMESTAMP;
    if (c >= 12 && c <= 13) return COUNTER;
    if (c >= 14) return BITMASK;
    return NUMERIC;
}

double numericValue(const TurbineRecord& r, int c) {
    switch (c) {
        case 1: return r.windspeed;
        case 2: return r.rpm;
        case 3: return r.voltageIn;
        case 4: return r.voltageL1;
        case 5: return r.voltageL2;
        case 6: return r.currentOut;
        case 7: return r.powerOut;
        case 8: return r.powerReg;
        case 9: return r.t1;
        case 10: return r.t2;
        case 11: return r.t3;
        case 12: return r.eventCount;
        case 13: return r.lastEventCode;
        default: return 0;
    }
}

int statusValue(const TurbineRecord& r, int c) {
    if (c == 14) return r.turbineStatus;
    if (c == 15) return r.gridStatus;
    if (c == 16) return r.systemStatus;
    return 0;
}

// ===================== TIMESTAMP HELPERS =====================

// Converts a calendar date+time into total seconds, so "nearest" means an
// actual numeric distance rather than alphabetical string order. Ignores
// leap-year adjustment -- fine since this dataset spans a single month.
long long toTotalSeconds(int year, int month, int day, int hour, int minute, int second) {
    static int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long long totalDays = (long long)year * 365;
    for (int m = 0; m < month - 1; m++) totalDays += daysInMonth[m];
    totalDays += (day - 1);
    return totalDays * 86400LL + hour * 3600 + minute * 60 + second;
}

// Stored Log Time format: "YYYY:MM:DD:HH:MM:SS,ms" (ms ignored).
long long parseLogTimeSeconds(const string& logTime) {
    return toTotalSeconds(
        stoi(logTime.substr(0, 4)), stoi(logTime.substr(5, 2)), stoi(logTime.substr(8, 2)),
        stoi(logTime.substr(11, 2)), stoi(logTime.substr(14, 2)), stoi(logTime.substr(17, 2)));
}

// User-entered date only: "YYYY:MM:DD" -> total days.
long long parseDateDays(const string& d) {
    static int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = stoi(d.substr(0, 4)), month = stoi(d.substr(5, 2)), day = stoi(d.substr(8, 2));
    long long totalDays = (long long)year * 365;
    for (int m = 0; m < month - 1; m++) totalDays += daysInMonth[m];
    return totalDays + (day - 1);
}

// User-entered date+time: "YYYY:MM:DD:HH:MM:SS" -> total seconds.
long long parseDateTimeSeconds(const string& dt) {
    return toTotalSeconds(
        stoi(dt.substr(0, 4)), stoi(dt.substr(5, 2)), stoi(dt.substr(8, 2)),
        stoi(dt.substr(11, 2)), stoi(dt.substr(14, 2)), stoi(dt.substr(17, 2)));
}

// ===================== FILTERS =====================

vector<int> numericFilter(const vector<TurbineRecord>& data,
                          const vector<int>& current, int column,
                          double a, double b, bool range, bool greater) {
    vector<int> result;
    for (int index : current) {
        double v = numericValue(data[index], column);
        bool match = range ? (v >= a && v <= b) :
                             (greater ? v > a : v < a);
        if (match) result.push_back(index);
    }
    return result;
}

vector<int> statusFilter(const vector<TurbineRecord>& data,
                         const vector<int>& current, int column,
                         int value, bool bitMode) {
    vector<int> result;
    for (int index : current) {
        int v = statusValue(data[index], column);
        if ((bitMode && hasBit(v, value)) || (!bitMode && v == value))
            result.push_back(index);
    }
    return result;
}

vector<int> dateTimeFilter(const vector<TurbineRecord>& data,
                           const vector<int>& current, bool dateMode,
                           const string& start, const string& end) {
    vector<int> result;
    for (int index : current) {
        const string& t = data[index].logTime;
        if (t.size() < 19) continue; // guards against a malformed timestamp
        string value = dateMode ? t.substr(0, 10) : t.substr(11, 8);
        if (value >= start && value <= end)
            result.push_back(index);
    }
    return result;
}

// Search by DATE ONLY: returns the whole day. If that exact date isn't
// present, falls back to whichever date IS present that's numerically
// closest (not just alphabetically next), and returns that whole day.
vector<int> searchByDate(const vector<TurbineRecord>& data,
                         const vector<int>& current, const string& targetDate) {
    vector<int> exact;
    for (int index : current)
        if (data[index].logTime.substr(0, 10) == targetDate)
            exact.push_back(index);
    if (!exact.empty() || current.empty()) return exact;

    long long targetDays = parseDateDays(targetDate);
    string nearestDate = data[current[0]].logTime.substr(0, 10);
    long long bestDiff = -1;

    for (int index : current) {
        string datePart = data[index].logTime.substr(0, 10);
        long long diff = abs(parseDateDays(datePart) - targetDays);
        if (bestDiff == -1 || diff < bestDiff) {
            bestDiff = diff;
            nearestDate = datePart;
        }
    }

    vector<int> result;
    for (int index : current)
        if (data[index].logTime.substr(0, 10) == nearestDate)
            result.push_back(index);
    return result;
}

// Search by DATE + TIME: returns the single closest-matching record,
// measured by actual elapsed seconds.
int searchByDateTime(const vector<TurbineRecord>& data,
                     const vector<int>& current, const string& targetDateTime) {
    if (current.empty()) return -1;
    long long target = parseDateTimeSeconds(targetDateTime);

    int best = current[0];
    long long bestDiff = llabs(parseLogTimeSeconds(data[best].logTime) - target);

    for (int index : current) {
        long long diff = llabs(parseLogTimeSeconds(data[index].logTime) - target);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = index;
        }
    }
    return best;
}

// ===================== SORT + SEARCH =====================

void insertionSort(const vector<TurbineRecord>& data, vector<int>& current,
                   int column, bool ascending) {
    ColumnType type = getColumnType(column);

    for (int i = 1; i < (int)current.size(); i++) {
        int key = current[i];
        int j = i - 1;

        while (j >= 0) {
            bool shift;

            if (type == TIMESTAMP) {
                shift = ascending
                    ? data[current[j]].logTime > data[key].logTime
                    : data[current[j]].logTime < data[key].logTime;
            } else {
                double left = (type == BITMASK)
                    ? statusValue(data[current[j]], column)
                    : numericValue(data[current[j]], column);
                double right = (type == BITMASK)
                    ? statusValue(data[key], column)
                    : numericValue(data[key], column);
                shift = ascending ? left > right : left < right;
            }

            if (!shift) break;
            current[j + 1] = current[j];
            j--;
        }
        current[j + 1] = key;
    }
}

int nearestSearch(const vector<TurbineRecord>& data,
                  const vector<int>& current, int column, double target) {
    if (current.empty()) return -1;

    int best = current[0];
    double bestDiff = fabs(numericValue(data[best], column) - target);

    for (int index : current) {
        double diff = fabs(numericValue(data[index], column) - target);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = index;
        }
    }
    return best;
}

int exactSearch(const vector<TurbineRecord>& data,
                const vector<int>& current, int column, int target) {
    for (int index : current)
        if (statusValue(data[index], column) == target)
            return index;
    return -1;
}

// ===================== STACK + WORKING SET =====================

const int MAX_OPERATIONS = 20;
Operation opStack[MAX_OPERATIONS];
int stackTop = -1;
vector<int> workingSet;

bool pushOperation(const Operation& op) {
    if (stackTop == MAX_OPERATIONS - 1) {
        cout << "Maximum of " << MAX_OPERATIONS << " operations reached.\n";
        return false;
    }
    opStack[++stackTop] = op;
    return true;
}

bool popOperation() {
    if (stackTop == -1) {
        cout << "No operation to go back from.\n";
        return false;
    }
    stackTop--;
    return true;
}

void resetWorkingSet(const vector<TurbineRecord>& data) {
    workingSet.clear();
    for (int i = 0; i < (int)data.size(); i++)
        workingSet.push_back(i);
}

vector<int> applyOperation(const vector<TurbineRecord>& data,
                           vector<int> current, const Operation& op) {
    switch (op.type) {
        case RANGE_FILTER:
            return numericFilter(data, current, op.column, op.value1, op.value2, true, true);

        case THRESHOLD_FILTER:
            return numericFilter(data, current, op.column, op.value1, 0,
                                 false, op.firstGreater);

        case EXACT_FILTER:
            return statusFilter(data, current, op.column, (int)op.value1, false);

        case BIT_FILTER:
            return statusFilter(data, current, op.column, (int)op.value1, true);

        case DATE_FILTER:
            return dateTimeFilter(data, current, true, op.text1, op.text2);

        case TIME_FILTER:
            return dateTimeFilter(data, current, false, op.text1, op.text2);

        case SORT_OPERATION:
            insertionSort(data, current, op.column, op.ascending);
            return current;

        case NEAREST_SEARCH: {
            int index = nearestSearch(data, current, op.column, op.value1);
            return index == -1 ? vector<int>() : vector<int>{index};
        }

        case EXACT_SEARCH: {
            int index = exactSearch(data, current, op.column, (int)op.value1);
            return index == -1 ? vector<int>() : vector<int>{index};
        }

        case TIMESTAMP_SEARCH:
            // "YYYY:MM:DD" is 10 characters -> date-only search (whole
            // day, nearest-date fallback). Longer -> full date+time ->
            // single closest match.
            if (op.text1.length() == 10) {
                return searchByDate(data, current, op.text1);
            } else {
                int index = searchByDateTime(data, current, op.text1);
                return index == -1 ? vector<int>() : vector<int>{index};
            }
    }
    return current;
}

void rebuild(const vector<TurbineRecord>& data) {
    resetWorkingSet(data);
    for (int i = 0; i <= stackTop; i++)
        workingSet = applyOperation(data, workingSet, opStack[i]);
}

// ===================== DISPLAY =====================

void printResults(const vector<TurbineRecord>& data) {
    if (workingSet.empty()) {
        cout << "No matching records.\n";
        return;
    }

    cout << "\nTotal matches: " << workingSet.size() << "\n";
    cout << left << setw(24) << "Log Time"
         << setw(12) << "Power out"
         << setw(12) << "Windspeed"
         << setw(10) << "T.Status" << '\n';

    int limit = min(20, (int)workingSet.size());
    for (int i = 0; i < limit; i++) {
        const TurbineRecord& r = data[workingSet[i]];
        cout << left << setw(24) << r.logTime
             << setw(12) << r.powerOut
             << setw(12) << r.windspeed
             << setw(10) << r.turbineStatus << '\n';
    }

    if ((int)workingSet.size() > 20)
        cout << "... " << workingSet.size() - 20 << " more rows not shown.\n";
}

// Shows what's actually in the CURRENT results for a column, before
// asking what operation to run -- so the user isn't guessing valid
// input values (e.g. typing a threshold outside the real data range).
void showColumnInfo(const vector<TurbineRecord>& data, int column) {
    ColumnType type = getColumnType(column);

    if (workingSet.empty()) {
        cout << "\nNo records match the current filters -- nothing to inspect here.\n";
        return;
    }

    if (type == NUMERIC || type == COUNTER) {
        double lo = numericValue(data[workingSet[0]], column);
        double hi = lo;
        for (int index : workingSet) {
            double v = numericValue(data[index], column);
            lo = min(lo, v);
            hi = max(hi, v);
        }
        cout << "\n" << columnNames[column] << " -- current range: " << lo << " to " << hi
             << "  (" << workingSet.size() << " matching rows)\n";

    } else if (type == BITMASK) {
        vector<int> seen;
        for (int index : workingSet) {
            int v = statusValue(data[index], column);
            if (find(seen.begin(), seen.end(), v) == seen.end())
                seen.push_back(v);
        }
        cout << "\n" << columnNames[column] << " -- status codes present: ";
        for (size_t i = 0; i < seen.size(); i++)
            cout << seen[i] << (i + 1 < seen.size() ? ", " : "");
        cout << "  (" << workingSet.size() << " matching rows)\n";

    } else { // TIMESTAMP
        string earliest = data[workingSet[0]].logTime, latest = earliest;
        for (int index : workingSet) {
            earliest = min(earliest, data[index].logTime);
            latest = max(latest, data[index].logTime);
        }
        cout << "\n" << columnNames[column] << " -- current span: " << earliest
             << " to " << latest << "  (" << workingSet.size() << " matching rows)\n";
    }
}

// ===================== INPUT + MENU =====================

int getInt(const string& prompt, int low, int high) {
    int value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (cin.fail()) {
            cout << "That's not a number. Please try again.\n";
            cin.clear();
            cin.ignore(1000, '\n');
            continue;
        }
        if (value < low || value > high) {
            cout << "Out of range -- please enter a number from " << low << " to " << high << ".\n";
            continue;
        }
        return value;
    }
}

double getDouble(const string& prompt) {
    double value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (!cin.fail()) return value;
        cout << "That's not a number. Please try again.\n";
        cin.clear();
        cin.ignore(1000, '\n');
    }
}

// Reads one character from an allowed set (e.g. "><" or "AD"),
// case-insensitive. Used instead of ambiguous "1 for X, 2 for Y" codes.
char getChar(const string& prompt, const string& allowed) {
    char value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (cin.fail()) {
            cout << "Invalid input. Please try again.\n";
            cin.clear();
            cin.ignore(1000, '\n');
            continue;
        }
        char upper = toupper(value);
        if (allowed.find(upper) != string::npos) return upper;
        cout << "Invalid choice. Please enter one of: " << allowed << "\n";
    }
}

void showColumns() {
    cout << "\n--- Columns ---\n";
    for (int i = 0; i < 17; i++)
        cout << i << ". " << columnNames[i] << '\n';
}

void showOperations(ColumnType type) {
    cout << "\n--- Operations ---\n";
    if (type == NUMERIC || type == COUNTER) {
        cout << "1. Range Filter\n2. Threshold Filter\n3. Sort\n4. Nearest-Value Search\n";
    } else if (type == BITMASK) {
        cout << "1. Exact Match Filter\n2. Bit-is-Set Filter\n3. Sort\n4. Exact Match Search\n";
    } else {
        cout << "1. Date Range Filter\n2. Time-of-Day Filter\n3. Sort\n"
             << "4. Search (a date for the whole day, or date+time for closest match)\n";
    }
    cout << "5. Back\n6. Change Column (keep current results)\n7. Main Menu\n";
}

bool createOperation(Operation& op, int column, ColumnType type, int choice) {
    op.column = column;

    if (type == NUMERIC || type == COUNTER) {
        if (choice == 1) {
            op.type = RANGE_FILTER;
            op.value1 = getDouble("Minimum: ");
            op.value2 = getDouble("Maximum: ");
            if (op.value1 > op.value2) {
                cout << "Minimum cannot be greater than maximum.\n";
                return false;
            }
        } else if (choice == 2) {
            op.type = THRESHOLD_FILTER;
            op.value1 = getDouble("Threshold: ");
            op.firstGreater = getChar("Enter '>' for greater than, or '<' for less than: ", "><") == '>';
        } else if (choice == 3) {
            op.type = SORT_OPERATION;
            op.ascending = getChar("Enter 'A' for Ascending, or 'D' for Descending: ", "AD") == 'A';
        } else if (choice == 4) {
            op.type = NEAREST_SEARCH;
            op.value1 = getDouble("Target value to find the nearest match: ");
        } else return false;
    }
    else if (type == BITMASK) {
        if (choice == 1) {
            op.type = EXACT_FILTER;
            op.value1 = getInt("Exact status code: ", 0, 100000);
        } else if (choice == 2) {
            op.type = BIT_FILTER;
            op.value1 = getInt("Bit value (e.g. 2 for Braking): ", 1, 100000);
        } else if (choice == 3) {
            op.type = SORT_OPERATION;
            op.ascending = getChar("Enter 'A' for Ascending, or 'D' for Descending: ", "AD") == 'A';
        } else if (choice == 4) {
            op.type = EXACT_SEARCH;
            op.value1 = getInt("Status code to find: ", 0, 100000);
        } else return false;
    }
    else { // TIMESTAMP
        if (choice == 1) {
            op.type = DATE_FILTER;
            cout << "Start date (YYYY:MM:DD): "; cin >> op.text1;
            cout << "End date (YYYY:MM:DD): "; cin >> op.text2;
        } else if (choice == 2) {
            op.type = TIME_FILTER;
            cout << "Start time (HH:MM:SS): "; cin >> op.text1;
            cout << "End time (HH:MM:SS): "; cin >> op.text2;
        } else if (choice == 3) {
            op.type = SORT_OPERATION;
            op.ascending = getChar("Enter 'A' for Ascending, or 'D' for Descending: ", "AD") == 'A';
        } else if (choice == 4) {
            op.type = TIMESTAMP_SEARCH;
            cout << "Enter a date (YYYY:MM:DD) for that whole day, "
                 << "or a date+time (YYYY:MM:DD:HH:MM:SS) for the closest match: ";
            cin >> op.text1;
        } else return false;
    }
    return true;
}

// ===================== COLUMN REPORT =====================

void columnReport(const vector<TurbineRecord>& data) {
    stackTop = -1;
    resetWorkingSet(data);
    int column = -1;

    while (true) {
        if (column == -1) {
            showColumns();
            column = getInt("Select column (0-16): ", 0, 16);
        }

        ColumnType type = getColumnType(column);
        showColumnInfo(data, column);
        showOperations(type);
        int choice = getInt("Choice: ", 1, 7);

        if (choice == 5) {
            if (popOperation()) {
                rebuild(data);
                cout << "Went back. Matches: " << workingSet.size() << '\n';
            }
            continue;
        }
        if (choice == 6) {
            // Keeps the stack and working set intact -- this is what
            // lets filters chain across DIFFERENT columns (AND logic).
            column = -1;
            continue;
        }
        if (choice == 7) {
            stackTop = -1;
            resetWorkingSet(data);
            return;
        }

        Operation op;
        if (!createOperation(op, column, type, choice))
            continue;

        if (pushOperation(op)) {
            rebuild(data);
            printResults(data);
        }
    }
}

// ===================== MAIN =====================

int main() {
    string filename = "swt_august_2022_week_16_22.csv";

    int skipped = 0;
    vector<TurbineRecord> data = loadData(filename, skipped);

    if (data.empty()) {
        cout << "No valid records loaded. Exiting.\n";
        return 1;
    }

    cout << "Loaded " << data.size() << " records, skipped "
         << skipped << " rows.\n";

    while (true) {
        cout << "\n========================================\n"
             << "   Wind Turbine SCADA Analysis\n"
             << "========================================\n"
             << "1. Column Report\n"
             << "2. Exit\n";

        int choice = getInt("Choice: ", 1, 2);
        if (choice == 2) break;
        columnReport(data);
    }

    cout << "Goodbye.\n";
    return 0;
}
