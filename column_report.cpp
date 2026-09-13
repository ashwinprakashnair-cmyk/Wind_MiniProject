#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
using namespace std;

// ===================== STRUCT + INGESTION (from ingestion.cpp) =====================

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
    if (rec.windspeed < 0 || rec.windspeed > 25) return false;
    if (rec.rpm < 0) return false;
    if (rec.powerOut < 0) return false;
    if (rec.t1 < -10 || rec.t1 > 60) return false;
    return true;
}

bool tryStod(const string& s, double& out) {
    try { out = stod(s); return true; }
    catch (...) { return false; }
}

bool tryStoi(const string& s, int& out) {
    try { out = stoi(s); return true; }
    catch (...) { return false; }
}

// Refactored out of the original main() so both ingestion and column
// report can share the same loading logic.
vector<TurbineRecord> loadData(string filename, int& skippedCount) {
    vector<TurbineRecord> records;
    skippedCount = 0;

    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error... Could not open file" << endl;
        return records;
    }

    string line;
    getline(file, line); // skip header

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
    return records;
}

// ===================== PART 1: COLUMN TYPE ENUM + LOOKUP =====================

enum ColumnType {
    NUMERIC,
    BITMASK,
    TIMESTAMP,
    COUNTER
};

ColumnType getColumnType(int columnIndex) {
    switch (columnIndex) {
        case 0: return TIMESTAMP;
        case 1: return NUMERIC;
        case 2: return NUMERIC;
        case 3: return NUMERIC;
        case 4: return NUMERIC;
        case 5: return NUMERIC;
        case 6: return NUMERIC;
        case 7: return NUMERIC;
        case 8: return NUMERIC;
        case 9: return NUMERIC;
        case 10: return NUMERIC;
        case 11: return NUMERIC;
        case 12: return COUNTER;
        case 13: return COUNTER;
        case 14: return BITMASK;
        case 15: return BITMASK;
        case 16: return BITMASK;
        default: return NUMERIC;
    }
}

// ===================== PART 2: COLUMN NAMES =====================

string columnNames[17] = {
    "Log Time", "Windspeed", "RPM", "Voltage In", "Voltage L1",
    "Voltage L2", "Current out", "Power out", "Power reg",
    "T1", "T2", "T3", "Event count", "Last event code",
    "Turbine status", "Grid status", "System status"
};

// ===================== PART 3: DATA ACCESS LAYER =====================

double getNumericValue(TurbineRecord rec, int columnIndex) {
    switch (columnIndex) {
        case 1: return rec.windspeed;
        case 2: return rec.rpm;
        case 3: return rec.voltageIn;
        case 4: return rec.voltageL1;
        case 5: return rec.voltageL2;
        case 6: return rec.currentOut;
        case 7: return rec.powerOut;
        case 8: return rec.powerReg;
        case 9: return rec.t1;
        case 10: return rec.t2;
        case 11: return rec.t3;
        case 12: return rec.eventCount;
        case 13: return rec.lastEventCode;
        default: return 0.0;
    }
}

int getBitmaskValue(TurbineRecord rec, int columnIndex) {
    switch (columnIndex) {
        case 14: return rec.turbineStatus;
        case 15: return rec.gridStatus;
        case 16: return rec.systemStatus;
        default: return 0;
    }
}

// ===================== PART 4: OPERATION STRUCT + STACK =====================

enum OperationType {
    FILTER_RANGE,
    FILTER_THRESHOLD,
    FILTER_EXACT,
    FILTER_BIT,
    FILTER_DATE_RANGE,
    FILTER_TIME_OF_DAY,
    SORT_OP,
    SEARCH_NEAREST,
    SEARCH_EXACT
};

struct Operation {
    OperationType type;
    int columnIndex;
    double value1;
    double value2;
    bool ascending;
    bool greaterThan;
    string strValue1;
    string strValue2;
};

const int MAX_DEPTH = 20;
Operation opStack[MAX_DEPTH];
int stackTop = -1;

bool pushOperation(Operation op) {
    if (stackTop >= MAX_DEPTH - 1) {
        cout << "Stack overflow: maximum operation depth (" << MAX_DEPTH
             << ") reached. Cannot chain further." << endl;
        return false;
    }
    stackTop++;
    opStack[stackTop] = op;
    return true;
}

bool popOperation() {
    if (stackTop < 0) {
        cout << "Already at the start of this chain." << endl;
        return false;
    }
    stackTop--;
    return true;
}

void clearStack() {
    stackTop = -1;
}

// ===================== PART 5: WORKING SET + REPLAY =====================

vector<int> workingSet;

void resetWorkingSet(vector<TurbineRecord>& master) {
    workingSet.clear();
    for (size_t i = 0; i < master.size(); i++) {
        workingSet.push_back((int)i);
    }
}

// Forward declarations (bodies in Part 6/7/8)
vector<int> filterByRange(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, double minVal, double maxVal);
vector<int> filterByThreshold(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, double value, bool greaterThan);
vector<int> filterByExactBitmask(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, int exactValue);
vector<int> filterByBitSet(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, int bitValue);
vector<int> filterByDateRange(vector<TurbineRecord>& master, vector<int> currentIndices, string startDate, string endDate);
vector<int> filterByTimeOfDay(vector<TurbineRecord>& master, vector<int> currentIndices, string startTime, string endTime);
void sortWorkingSet(vector<TurbineRecord>& master, vector<int>& currentIndices, int columnIndex, bool ascending);
int searchNearestValue(vector<TurbineRecord>& master, vector<int>& currentIndices, int columnIndex, double target);
int searchExactBitmask(vector<TurbineRecord>& master, vector<int>& currentIndices, int columnIndex, int exactValue);

vector<int> applyOperation(vector<TurbineRecord>& master, vector<int> currentIndices, Operation op) {
    switch (op.type) {
        case FILTER_RANGE:
            return filterByRange(master, currentIndices, op.columnIndex, op.value1, op.value2);
        case FILTER_THRESHOLD:
            return filterByThreshold(master, currentIndices, op.columnIndex, op.value1, op.greaterThan);
        case FILTER_EXACT:
            return filterByExactBitmask(master, currentIndices, op.columnIndex, (int)op.value1);
        case FILTER_BIT:
            return filterByBitSet(master, currentIndices, op.columnIndex, (int)op.value1);
        case FILTER_DATE_RANGE:
            return filterByDateRange(master, currentIndices, op.strValue1, op.strValue2);
        case FILTER_TIME_OF_DAY:
            return filterByTimeOfDay(master, currentIndices, op.strValue1, op.strValue2);
        case SORT_OP:
            sortWorkingSet(master, currentIndices, op.columnIndex, op.ascending);
            return currentIndices;
        case SEARCH_NEAREST: {
            int idx = searchNearestValue(master, currentIndices, op.columnIndex, op.value1);
            vector<int> result;
            if (idx != -1) result.push_back(idx);
            return result;
        }
        case SEARCH_EXACT: {
            int idx = searchExactBitmask(master, currentIndices, op.columnIndex, (int)op.value1);
            vector<int> result;
            if (idx != -1) result.push_back(idx);
            return result;
        }
    }
    return currentIndices;
}

// Rebuilds workingSet from scratch by replaying every operation on the
// stack, in order, starting from ALL master indices. No caching/snapshots
// are stored anywhere else — this is the only place workingSet changes.
void rebuildWorkingSet(vector<TurbineRecord>& master) {
    resetWorkingSet(master);
    for (int i = 0; i <= stackTop; i++) {
        workingSet = applyOperation(master, workingSet, opStack[i]);
    }
}

// ===================== PART 6: FILTER FUNCTIONS =====================

vector<int> filterByRange(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, double minVal, double maxVal) {
    vector<int> result;
    for (size_t i = 0; i < currentIndices.size(); i++) {
        double val = getNumericValue(master[currentIndices[i]], columnIndex);
        if (val >= minVal && val <= maxVal) {
            result.push_back(currentIndices[i]);
        }
    }
    return result;
}

vector<int> filterByThreshold(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, double value, bool greaterThan) {
    vector<int> result;
    for (size_t i = 0; i < currentIndices.size(); i++) {
        double val = getNumericValue(master[currentIndices[i]], columnIndex);
        bool matches = greaterThan ? (val > value) : (val < value);
        if (matches) {
            result.push_back(currentIndices[i]);
        }
    }
    return result;
}

vector<int> filterByExactBitmask(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, int exactValue) {
    vector<int> result;
    for (size_t i = 0; i < currentIndices.size(); i++) {
        int val = getBitmaskValue(master[currentIndices[i]], columnIndex);
        if (val == exactValue) {
            result.push_back(currentIndices[i]);
        }
    }
    return result;
}

vector<int> filterByBitSet(vector<TurbineRecord>& master, vector<int> currentIndices, int columnIndex, int bitValue) {
    vector<int> result;
    for (size_t i = 0; i < currentIndices.size(); i++) {
        int val = getBitmaskValue(master[currentIndices[i]], columnIndex);
        if (hasBit(val, bitValue)) {
            result.push_back(currentIndices[i]);
        }
    }
    return result;
}

// Log Time format: 2022:08:16:00:05:42,537
// Date part = first 10 chars ("2022:08:16"), directly string-comparable
// since it's fixed-width (no need to parse into a date object).
vector<int> filterByDateRange(vector<TurbineRecord>& master, vector<int> currentIndices, string startDate, string endDate) {
    vector<int> result;
    for (size_t i = 0; i < currentIndices.size(); i++) {
        string datePart = master[currentIndices[i]].logTime.substr(0, 10);
        if (datePart >= startDate && datePart <= endDate) {
            result.push_back(currentIndices[i]);
        }
    }
    return result;
}

// Time part = chars 11-18 ("HH:MM:SS"), same fixed-width string logic.
vector<int> filterByTimeOfDay(vector<TurbineRecord>& master, vector<int> currentIndices, string startTime, string endTime) {
    vector<int> result;
    for (size_t i = 0; i < currentIndices.size(); i++) {
        string timePart = master[currentIndices[i]].logTime.substr(11, 8);
        if (timePart >= startTime && timePart <= endTime) {
            result.push_back(currentIndices[i]);
        }
    }
    return result;
}

// ===================== PART 7: SORT (self-written, insertion sort) =====================

void sortWorkingSet(vector<TurbineRecord>& master, vector<int>& currentIndices, int columnIndex, bool ascending) {
    ColumnType type = getColumnType(columnIndex);

    for (size_t i = 1; i < currentIndices.size(); i++) {
        int key = currentIndices[i];
        int j = (int)i - 1;

        while (j >= 0) {
            int compareIndex = currentIndices[j];
            bool shouldSwap;

            if (type == TIMESTAMP) {
                shouldSwap = ascending
                    ? (master[compareIndex].logTime > master[key].logTime)
                    : (master[compareIndex].logTime < master[key].logTime);
            } else {
                double keyVal = (type == BITMASK)
                    ? getBitmaskValue(master[key], columnIndex)
                    : getNumericValue(master[key], columnIndex);
                double compareVal = (type == BITMASK)
                    ? getBitmaskValue(master[compareIndex], columnIndex)
                    : getNumericValue(master[compareIndex], columnIndex);
                shouldSwap = ascending ? (compareVal > keyVal) : (compareVal < keyVal);
            }

            if (!shouldSwap) break;
            currentIndices[j + 1] = currentIndices[j];
            j--;
        }
        currentIndices[j + 1] = key;
    }
}

// ===================== PART 8: SEARCH (self-written) =====================

int searchNearestValue(vector<TurbineRecord>& master, vector<int>& currentIndices, int columnIndex, double target) {
    if (currentIndices.empty()) return -1;

    int bestIndex = currentIndices[0];
    double bestDiff = getNumericValue(master[bestIndex], columnIndex) - target;
    if (bestDiff < 0) bestDiff = -bestDiff;

    for (size_t i = 1; i < currentIndices.size(); i++) {
        double val = getNumericValue(master[currentIndices[i]], columnIndex);
        double diff = val - target;
        if (diff < 0) diff = -diff;
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIndex = currentIndices[i];
        }
    }
    return bestIndex;
}

int searchExactBitmask(vector<TurbineRecord>& master, vector<int>& currentIndices, int columnIndex, int exactValue) {
    for (size_t i = 0; i < currentIndices.size(); i++) {
        if (getBitmaskValue(master[currentIndices[i]], columnIndex) == exactValue) {
            return currentIndices[i];
        }
    }
    return -1;
}

// ===================== PART 9: DISPLAY =====================

const int DISPLAY_LIMIT = 20;

void printWorkingSet(vector<TurbineRecord>& master, vector<int>& currentIndices) {
    if (currentIndices.empty()) {
        cout << "No matching records." << endl;
        return;
    }

    cout << "Total matches: " << currentIndices.size() << endl;
    cout << left << setw(24) << "Log Time"
         << setw(12) << "Power out"
         << setw(12) << "Windspeed"
         << setw(10) << "T.Status" << endl;

    int limit = min((int)currentIndices.size(), DISPLAY_LIMIT);
    for (int i = 0; i < limit; i++) {
        TurbineRecord& rec = master[currentIndices[i]];
        cout << left << setw(24) << rec.logTime
             << setw(12) << rec.powerOut
             << setw(12) << rec.windspeed
             << setw(10) << rec.turbineStatus << endl;
    }

    if ((int)currentIndices.size() > DISPLAY_LIMIT) {
        cout << "... (" << currentIndices.size() - DISPLAY_LIMIT << " more rows not shown)" << endl;
    }
}

// ===================== PART 10: MENU / NAVIGATION =====================

int getValidatedInt(string prompt, int minVal, int maxVal) {
    int value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (cin.fail() || value < minVal || value > maxVal) {
            cout << "Invalid input. Please enter a number between "
                 << minVal << " and " << maxVal << "." << endl;
            cin.clear();
            cin.ignore(1000, '\n');
            continue;
        }
        break;
    }
    return value;
}

double getValidatedDouble(string prompt) {
    double value;
    while (true) {
        cout << prompt;
        cin >> value;
        if (cin.fail()) {
            cout << "Invalid input. Please enter a valid number." << endl;
            cin.clear();
            cin.ignore(1000, '\n');
            continue;
        }
        break;
    }
    return value;
}

void showColumnList() {
    cout << "\n--- Select a Column ---" << endl;
    for (int i = 0; i < 17; i++) {
        cout << i << ". " << columnNames[i] << endl;
    }
}

void showOperationsMenu(ColumnType type) {
    cout << "\n--- Choose Operation ---" << endl;
    if (type == NUMERIC || type == COUNTER) {
        cout << "1. Range Filter" << endl;
        cout << "2. Threshold Filter" << endl;
        cout << "3. Sort" << endl;
        cout << "4. Nearest-Value Search" << endl;
    } else if (type == BITMASK) {
        cout << "1. Exact Match Filter" << endl;
        cout << "2. Bit-is-Set Filter" << endl;
        cout << "3. Sort by Code" << endl;
        cout << "4. Exact Match Search" << endl;
    } else if (type == TIMESTAMP) {
        cout << "1. Date Range Filter" << endl;
        cout << "2. Time-of-Day Filter" << endl;
        cout << "3. Chronological Sort" << endl;
        cout << "(4 not available for this column type)" << endl;
    }
    cout << "5. Back" << endl;
    cout << "6. Change Column (keep current results)" << endl;
    cout << "7. Main Menu" << endl;
}

void runColumnReport(vector<TurbineRecord>& master) {
    clearStack();
    resetWorkingSet(master);

    bool inColumnReport = true;
    int selectedColumn = -1;

    while (inColumnReport) {
        if (selectedColumn == -1) {
            showColumnList();
            selectedColumn = getValidatedInt("Enter column number (0-16): ", 0, 16);
        }

        ColumnType type = getColumnType(selectedColumn);
        showOperationsMenu(type);

        int opChoice = getValidatedInt("Enter choice: ", 1, 7);

        if (opChoice == 5) {
            if (popOperation()) {
                rebuildWorkingSet(master);
                cout << "Went back one step. Current matches: " << workingSet.size() << endl;
            }
            continue;
        }
        if (opChoice == 6) {
            // Change column WITHOUT resetting the stack or working set —
            // this is what makes multi-column chaining (AND logic across
            // columns) actually reachable from the menu.
            selectedColumn = -1;
            continue;
        }
        if (opChoice == 7) {
            clearStack();
            resetWorkingSet(master);
            selectedColumn = -1;
            inColumnReport = false;
            continue;
        }
        if (type == TIMESTAMP && opChoice == 4) {
            cout << "That operation is not available for a Timestamp column." << endl;
            continue;
        }

        Operation op;
        op.columnIndex = selectedColumn;

        if (type == NUMERIC || type == COUNTER) {
            if (opChoice == 1) {
                double minV = getValidatedDouble("Enter min value: ");
                double maxV = getValidatedDouble("Enter max value: ");
                if (minV > maxV) {
                    cout << "Invalid range: min cannot be greater than max." << endl;
                    continue;
                }
                op.type = FILTER_RANGE;
                op.value1 = minV;
                op.value2 = maxV;
            } else if (opChoice == 2) {
                      double val = getValidatedDouble("Enter threshold value: ");
                int dir = getValidatedInt("1 for Greater Than, 2 for Less Than: ", 1, 2);
                op.type = FILTER_THRESHOLD;
                op.value1 = val;
                op.greaterThan = (dir == 1);
            } else if (opChoice == 3) {
                int dir = getValidatedInt("1 for Ascending, 2 for Descending: ", 1, 2);
                op.type = SORT_OP;
                op.ascending = (dir == 1);
            } else if (opChoice == 4) {
                double target = getValidatedDouble("Enter target value: ");
                op.type = SEARCH_NEAREST;
                op.value1 = target;
            }
        } else if (type == BITMASK) {
            if (opChoice == 1) {
                int val = getValidatedInt("Enter exact status code: ", 0, 100000);
                op.type = FILTER_EXACT;
                op.value1 = val;
            } else if (opChoice == 2) {
                int bitVal = getValidatedInt("Enter bit value (e.g. 2 for Braking): ", 1, 100000);
                op.type = FILTER_BIT;
                op.value1 = bitVal;
            } else if (opChoice == 3) {
                int dir = getValidatedInt("1 for Ascending, 2 for Descending: ", 1, 2);
                op.type = SORT_OP;
                op.ascending = (dir == 1);
            } else if (opChoice == 4) {
                int val = getValidatedInt("Enter exact status code to find: ", 0, 100000);
                op.type = SEARCH_EXACT;
                op.value1 = val;
            }
        } else if (type == TIMESTAMP) {
            if (opChoice == 1) {
                string startD, endD;
                cout << "Enter start date (YYYY:MM:DD): "; cin >> startD;
                cout << "Enter end date (YYYY:MM:DD): "; cin >> endD;
                op.type = FILTER_DATE_RANGE;
                op.strValue1 = startD;
                op.strValue2 = endD;
            } else if (opChoice == 2) {
                string startT, endT;
                cout << "Enter start time (HH:MM:SS): "; cin >> startT;
                cout << "Enter end time (HH:MM:SS): "; cin >> endT;
                op.type = FILTER_TIME_OF_DAY;
                op.strValue1 = startT;
                op.strValue2 = endT;
            } else if (opChoice == 3) {
                int dir = getValidatedInt("1 for Ascending, 2 for Descending: ", 1, 2);
                op.type = SORT_OP;
                op.ascending = (dir == 1);
            }
        }

        if (pushOperation(op)) {
            rebuildWorkingSet(master);
            printWorkingSet(master, workingSet);
        }
    }
}

void showMainMenu() {
    cout << "\n========================================" << endl;
    cout << "   Wind Turbine SCADA Analysis (Mini)" << endl;
    cout << "========================================" << endl;
    cout << "1. Column Report (Filter/Sort/Search)" << endl;
    cout << "2. Exit" << endl;
}

int main() {
    string filename;
    cout << "Enter dataset filename: ";
    cin >> filename;

    int skippedCount = 0;
    vector<TurbineRecord> records = loadData(filename, skippedCount);
    if (records.empty()) {
        cout << "No records loaded. Exiting." << endl;
        return 1;
    }
    cout << "Loaded " << records.size() << " records, skipped " << skippedCount << " rows." << endl;

    bool running = true;
    while (running) {
        showMainMenu();
        int choice = getValidatedInt("Enter choice: ", 1, 2);
        if (choice == 1) {
            runColumnReport(records);
        } else {
            running = false;
        }
    }
    cout << "Goodbye." << endl;
    return 0;
}
