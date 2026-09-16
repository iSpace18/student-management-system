#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <regex>
#include <limits>
#include <algorithm>
#include <ctime>

using namespace std;

enum Role { ADMIN, TEACHER, STUDENT };
enum EducationType { BUDGET, PAID };

class Student {
public:
    int id = 0;
    string first_name, last_name, group;
    int course = 0;
    double avg = 0.0;
    EducationType education_type = BUDGET;
    string password = "123";

    vector<string> grades, homeworks, attendance, schedule;

    Student() = default;
    Student(int i, string fn, string ln, string g, int c, EducationType et)
        : id(i), first_name(move(fn)), last_name(move(ln)), group(move(g)),
        course(c), education_type(et) {
    }
};

static vector<Student> students;
static int next_id = 1;
static Role current_role = ADMIN;
static int current_student_id = -1;

static const char* DB_FILE = "students.db";
static const string daysOfWeek[] = {
    "Понедельник","Вторник","Среда","Четверг","Пятница","Суббота","Воскресенье"
};

 ---------- ВАЛИДАЦИЯ ----------
static bool isValidName(const string& s) {
    static const regex r("^[А-Яа-яЁё\\s-]+$");
    return regex_match(s, r) && s.size() >= 2 && s.size() <= 50;
}
static bool isValidGroup(const string& s) {
    static const regex r("^[А-Яа-я0-9-]{2,10}$");
    return regex_match(s, r);
}
static bool isValidDate(const string& s) {
    static const regex r("^([0-2][0-9]|3[0-1])\\.(0[1-9]|1[0-2])\\.(19|20)\\d{2}$");
    return regex_match(s, r);
}
static bool isValidTime(const string& s) {
    static const regex r("^([0-1][0-9]|2[0-3]):[0-5][0-9]$");
    return regex_match(s, r);
}

 ---------- ВВОД ----------
static string askLine(const string& prompt, bool required = true) {
    while (true) {
        cout << prompt;
        string s;
        getline(cin, s);
        if (!required || !s.empty()) return s;
        cout << "Ошибка: Поле не может быть пустым!\n";
    }
}

static int askInt(const string& prompt, int minVal = numeric_limits<int>::min(),
    int maxVal = numeric_limits<int>::max()) {
    while (true) {
        string line = askLine(prompt);
        stringstream ss(line);
        int n;
        if (!(ss >> n) || !ss.eof()) { cout << "Ошибка: Неверный формат числа!\n"; continue; }
        if (n < minVal || n > maxVal) {
            cout << "Ошибка: Число должно быть от " << minVal << " до " << maxVal << "\n";
            continue;
        }
        return n;
    }
}

static string askName(const string& prompt) {
    while (true) {
        string s = askLine(prompt);
        if (isValidName(s)) return s;
        cout << "Ошибка: Имя/фамилия должны содержать только русские буквы (2-50 символов)!\n";
    }
}
static string askGroup(const string& prompt) {
    while (true) {
        string s = askLine(prompt);
        if (isValidGroup(s)) return s;
        cout << "Ошибка: Группа: 2-10 символов (буквы, цифры, дефис)\n";
    }
}
static string askDate(const string& prompt) {
    while (true) {
        string s = askLine(prompt);
        if (isValidDate(s)) return s;
        cout << "Ошибка: Дата в формате дд.мм.гггг (01.01.2025)\n";
    }
}
static string askTime(const string& prompt) {
    while (true) {
        string s = askLine(prompt);
        if (isValidTime(s)) return s;
        cout << "Ошибка: Время в формате чч:мм (08:30)\n";
    }
}

static int askDayOfWeek() {
    cout << "День недели:\n";
    for (int i = 0; i < 7; ++i) cout << (i + 1) << ". " << daysOfWeek[i] << "\n";
    return askInt("Выберите (1-7): ", 1, 7);
}

 ---------- УТИЛИТЫ ----------
static bool hasAccess(Role r) { return current_role == r || current_role == ADMIN; }
static bool hasAdminAccess() { return current_role == ADMIN; }

static int idxById(int id) {
    for (int i = 0; i < (int)students.size(); ++i)
        if (students[i].id == id) return i;
    return -1;
}

static string getDate() {
    time_t t = time(nullptr);
    tm now{};
#ifdef _WIN32
    localtime_s(&now, &t);
#else
    now = *localtime(&t);
#endif
    char buf[11]{};
    strftime(buf, sizeof(buf), "%d.%m.%Y", &now);
    return string(buf);
}

static void recalcAvg(Student& s) {
    double sum = 0.0;
    int count = 0;
    for (const string& grade : s.grades) {
        size_t p1 = grade.find(": ");
        size_t p2 = grade.find(" (");
        if (p1 != string::npos && p2 != string::npos && p1 + 2 < p2) {
            sum += stoi(grade.substr(p1 + 2, p2 - (p1 + 2)));
            count++;
        }
    }
    s.avg = (count > 0) ? (sum / count) : 0.0;
}

 ---------- СЕРИАЛИЗАЦИЯ (length-prefix) ----------
static void writeString(ostream& os, const string& s) {
    os << s.size() << "\n";
    os.write(s.data(), (streamsize)s.size());
    os << "\n";
}

static bool readString(istream& is, string& out) {
    string line;
    if (!getline(is, line)) return false;
    size_t len = 0;
    try { len = (size_t)stoul(line); }
    catch (...) { return false; }

    out.assign(len, '\0');
    if (len && !is.read(&out[0], (streamsize)len)) return false;

    char nl = 0;
    return (bool)is.get(nl) && nl == '\n';
}

static void writeVec(ostream& os, const vector<string>& v) {
    os << v.size() << "\n";
    for (auto& s : v) writeString(os, s);
}

static bool readVec(istream& is, vector<string>& v) {
    string line;
    if (!getline(is, line)) return false;
    size_t n = 0;
    try { n = (size_t)stoul(line); }
    catch (...) { return false; }

    v.clear();
    v.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        string s;
        if (!readString(is, s)) return false;
        v.push_back(move(s));
    }
    return true;
}

static void saveData() {
    ofstream file(DB_FILE, ios::binary);
    if (!file) return;

    file << "STUDENTS_DB_V1\n";
    file << next_id << "\n";
    file << students.size() << "\n";

    for (const Student& s : students) {
        file << s.id << "\n";
        writeString(file, s.first_name);
        writeString(file, s.last_name);
        writeString(file, s.group);
        file << s.course << "\n";
        file << fixed << setprecision(2) << s.avg << "\n";
        file << (int)s.education_type << "\n";
        writeString(file, s.password);

        writeVec(file, s.grades);
        writeVec(file, s.homeworks);
        writeVec(file, s.attendance);
        writeVec(file, s.schedule);
    }
}

static void loadData() {
    ifstream file(DB_FILE, ios::binary);
    if (!file) return;

    string header;
    if (!getline(file, header) || header != "STUDENTS_DB_V1") return;

    string line;
    if (!getline(file, line)) return;
    next_id = stoi(line);

    if (!getline(file, line)) return;
    int count = stoi(line);

    students.clear();
    students.reserve(count);

    for (int i = 0; i < count; ++i) {
        Student s;

        if (!getline(file, line)) return;
        s.id = stoi(line);

        if (!readString(file, s.first_name)) return;
        if (!readString(file, s.last_name)) return;
        if (!readString(file, s.group)) return;

        if (!getline(file, line)) return; s.course = stoi(line);
        if (!getline(file, line)) return; s.avg = stod(line);
        if (!getline(file, line)) return; s.education_type = (EducationType)stoi(line);
        if (!readString(file, s.password)) return;

        if (!readVec(file, s.grades)) return;
        if (!readVec(file, s.homeworks)) return;
        if (!readVec(file, s.attendance)) return;
        if (!readVec(file, s.schedule)) return;

        students.push_back(move(s));
    }

    int maxId = 0;
    for (auto& s : students) maxId = max(maxId, s.id);
    next_id = max(next_id, maxId + 1);
}

 ---------- ФУНКЦИИ ПРОГРАММЫ ----------
static void login() {
    cout << "Выберите роль: 1-Админ, 2-Преподаватель, 3-Студент: ";
    int r = askInt("", 1, 3);

    if (r == 1) {
        current_role = ADMIN; current_student_id = -1;
        cout << "Вход выполнен как админ\n";
    }
    else if (r == 2) {
        current_role = TEACHER; current_student_id = -1;
        cout << "Вход выполнен как преподаватель\n";
    }
    else {
        current_role = STUDENT;
        int id = askInt("Введите ID студента: ", 1);
        string pwd = askLine("Введите пароль: ");
        int idx = idxById(id);
        if (idx != -1 && students[idx].password == pwd) {
            current_student_id = id;
            cout << "Вход выполнен\n";
        }
        else {
            cout << "Ошибка входа\n";
            current_student_id = -1;
            current_role = ADMIN;
        }
    }
}

static void addStudent() {
    if (!hasAdminAccess()) { cout << "Доступ запрещён\n"; return; }

    string fn = askName("Введите имя: ");
    string ln = askName("Введите фамилию: ");
    string grp = askGroup("Введите группу: ");
    int crs = askInt("Введите курс (1-6): ", 1, 6);
    int etype = askInt("Тип обучения (1-Бюджет, 2-Платное): ", 1, 2);

    students.emplace_back(next_id, fn, ln, grp, crs, (etype == 1) ? BUDGET : PAID);
    cout << "Студент добавлен с ID: " << next_id << "\n";
    next_id++;
}

static void deleteStudent() {
    if (!hasAdminAccess()) { cout << "Доступ запрещён\n"; return; }

    int id = askInt("Введите ID студента для удаления: ", 1);
    int idx = idxById(id);
    if (idx == -1) { cout << "Студент не найден\n"; return; }

    cout << "Удаляется: " << students[idx].last_name << " " << students[idx].first_name << "\n";
    int confirm = askInt("Подтвердить (1-Да): ", 1, 1);
    if (confirm == 1) {
        students.erase(students.begin() + idx);
        cout << "Студент удалён\n";
    }
}

static void addSchedule() {
    if (!hasAdminAccess()) { cout << "Доступ запрещён\n"; return; }

    string grp = askGroup("Введите группу: ");
    int dayNum = askDayOfWeek();
    string time = askTime("Введите время (чч:мм): ");
    string subj = askLine("Введите предмет: ");
    string room = askLine("Введите аудиторию: ");

    bool found = false;
    for (Student& s : students) if (s.group == grp) {
        s.schedule.push_back(daysOfWeek[dayNum - 1] + " " + time + " - " + subj + " (" + room + ")");
        found = true;
    }
    cout << (found ? "Расписание добавлено для группы " + grp : "Группа не найдена") << "\n";
}

static void showAllStudents() {
    if (!hasAccess(TEACHER)) { cout << "Доступ запрещён\n"; return; }
    if (students.empty()) { cout << "Студентов нет\n"; return; }

    cout << fixed << setprecision(2);
    cout << "\nID | Фамилия Имя | Группа | Курс | Тип | Средний балл\n";
    cout << string(60, '-') << "\n";

    for (const Student& s : students) {
        string etype = (s.education_type == BUDGET) ? "Бюджет" : "Платное";
        cout << setw(3) << s.id << " | " << setw(15) << (s.last_name + " " + s.first_name.substr(0, 1))
            << " | " << setw(8) << s.group << " | " << setw(4) << s.course
            << " | " << setw(8) << etype << " | " << setw(6) << s.avg << "\n";
    }
}

static void showStudentGradesByIdx(int i) {
    const Student& s = students[i];
    if (s.grades.empty()) { cout << "Оценок нет\n"; return; }

    cout << "\n=== Оценки " << s.last_name << " " << s.first_name << " ===\n";
    for (const string& g : s.grades) cout << g << "\n";
    cout << "Средний балл: " << fixed << setprecision(2) << s.avg << "\n";
}

static void showAllStudentsGrades() {
    if (!hasAccess(TEACHER)) { cout << "Доступ запрещён\n"; return; }
    if (students.empty()) { cout << "Студентов нет\n"; return; }

    for (int i = 0; i < (int)students.size(); ++i) {
        showStudentGradesByIdx(i);
        cout << string(40, '-') << "\n";
    }
}

static void addGrade() {
    if (!hasAccess(TEACHER)) { cout << "Доступ запрещён\n"; return; }

    int id = askInt("Введите ID студента: ", 1);
    int idx = idxById(id);
    if (idx == -1) { cout << "Студент не найден\n"; return; }

    string subj = askLine("Введите предмет: ");
    int gr = askInt("Введите оценку (1-5): ", 1, 5);
    students[idx].grades.push_back(subj + ": " + to_string(gr) + " (" + getDate() + ")");
    recalcAvg(students[idx]);

    cout << "Оценка добавлена. Новый средний балл: " << fixed << setprecision(2) << students[idx].avg << "\n";
}

static void addHomework() {
    if (!hasAccess(TEACHER)) { cout << "Доступ запрещён\n"; return; }

    string grp = askGroup("Введите группу: ");
    string subj = askLine("Введите предмет: ");
    string desc = askLine("Введите описание ДЗ: ");
    string deadline = askDate("Введите срок сдачи (дд.мм.гггг): ");

    bool found = false;
    for (Student& s : students) if (s.group == grp) {
        s.homeworks.push_back(subj + ": " + desc + " (Срок: " + deadline + ")");
        found = true;
    }
    cout << (found ? "ДЗ добавлено для группы " + grp : "Группа не найдена") << "\n";
}

static void markAttendance() {
    if (!hasAccess(TEACHER)) { cout << "Доступ запрещён\n"; return; }

    string date = askDate("Введите дату (дд.мм.гггг): ");
    string subj = askLine("Введите предмет: ");
    string grp = askGroup("Введите группу: ");

    bool found = false;
    for (Student& s : students) if (s.group == grp) {
        string ans;
        while (true) {
            cout << s.last_name << " " << s.first_name << " присутствовал? (y/n): ";
            getline(cin, ans);
            if (ans == "y" || ans == "Y" || ans == "n" || ans == "N" || ans == "да" || ans == "нет") break;
            cout << "Введите y/n или да/нет\n";
        }
        string status = (ans == "y" || ans == "Y" || ans == "да") ? "Присутствовал" : "Отсутствовал";
        s.attendance.push_back(date + " - " + subj + ": " + status);
        found = true;
    }
    cout << (found ? "Посещаемость отмечена для группы " + grp : "Группа не найдена") << "\n";
}

static void viewList(const vector<string>& data, const string& title) {
    if (data.empty()) { cout << title << " отсутствуют\n"; return; }
    cout << "\n=== " << title << " ===\n";
    for (auto& x : data) cout << x << "\n";
}

static void showStudentData() {
    int idx = idxById(current_student_id);
    if (idx == -1) { cout << "Сначала войдите как студент\n"; return; }

    Student& s = students[idx];
    viewList(s.grades, "Ваши оценки");
    viewList(s.homeworks, "Ваши ДЗ");
    viewList(s.schedule, "Ваше расписание");
    viewList(s.attendance, "Ваша посещаемость");
}

static void showMenu() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    cout << "\n=== СИСТЕМА УЧЕТА СТУДЕНТОВ ===\n";
    cout << "Текущая роль: ";
    if (current_role == ADMIN) cout << "АДМИН";
    else if (current_role == TEACHER) cout << "ПРЕПОДАВАТЕЛЬ";
    else cout << "СТУДЕНТ (ID: " << current_student_id << ")";
    cout << "\n" << string(40, '=') << "\n\n";

    cout << "0. Сменить роль\n";
    if (hasAdminAccess()) {
        cout << "1. Добавить студента\n";
        cout << "2. Удалить студента\n";
        cout << "3. Добавить расписание\n";
    }
    if (hasAccess(TEACHER)) {
        cout << "4. Показать студентов\n";
        cout << "5. Показать оценки всех студентов\n";
        cout << "6. Добавить оценку\n";
        cout << "7. Добавить ДЗ\n";
        cout << "8. Отметить посещаемость\n";
    }
    if (current_role == STUDENT) cout << "9. Мои данные\n";

    cout << "\n99. Выход\n> ";
}

int main() {
#ifdef _WIN32
    system("chcp 1251 > nul");
#endif

    loadData();



    while (true) {
        showMenu();
        int choice = askInt("");

        switch (choice) {
        case 0: login(); break;
        case 1: addStudent(); break;
        case 2: deleteStudent(); break;
        case 3: addSchedule(); break;
        case 4: showAllStudents(); break;
        case 5: showAllStudentsGrades(); break;
        case 6: addGrade(); break;
        case 7: addHomework(); break;
        case 8: markAttendance(); break;
        case 9: if (current_role == STUDENT) showStudentData(); else cout << "Нет доступа\n"; break;
        case 99: saveData(); return 0;
        default: cout << "Неверный выбор\n"; break;
        }

        saveData();
        cout << "\nНажмите Enter для продолжения...";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        string dummy; getline(cin, dummy);
    }
}
