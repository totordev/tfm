#include <ncurses.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <set>
#include <algorithm>
#include <chrono>
#include <iomanip>

using namespace std;
namespace fs = filesystem;

set<string> markedFiles;
string filterQuery = "";  // Stores user input for filtering

string getUserName() {
    #ifdef _WIN32
        return getenv("USERNAME") ? getenv("USERNAME") : "Unknown";
    #else
        return getenv("USER") ? getenv("USER") : "Unknown";
    #endif
}

string getHomePath() {
    string currentUser = getUserName();
    stringstream unformattedCurrentPath;
    unformattedCurrentPath << "/home/" << currentUser;
    return unformattedCurrentPath.str();
}

string currentPath = getHomePath();

int selectedIndex = 0;
int scrollOffset = 0;

vector<string> getFiles(const string& directory, const string& filterQuery = "") {
    vector<string> files;

    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            string filename = entry.path().filename().string();

            // Apply filtering
            if (!filterQuery.empty() && filename.find(filterQuery) == string::npos) {
                continue;  // Skip non-matching files
            }

            files.push_back(filename);
        }

        // Sorting logic
        sort(files.begin(), files.end(), [](const string& a, const string& b) {
            string pathA = currentPath + "/" + a;
            string pathB = currentPath + "/" + b;

            bool isDirA = fs::is_directory(pathA);
            bool isDirB = fs::is_directory(pathB);
            bool isHiddenA = !a.empty() && a[0] == '.';
            bool isHiddenB = !b.empty() && b[0] == '.';

            // 1. Hidden folders first, then normal folders, then hidden files, then normal files
            if (isDirA != isDirB) return isDirA > isDirB; // Directories before files
            if (isHiddenA != isHiddenB) return isHiddenA > isHiddenB; // Hidden before non-hidden
            
            // 2. Alphabetical sorting within groups
            return a < b;
        });

    } catch (const exception& e) {
        mvprintw(0, 0, "Error reading directory: %s", e.what());
    }

    return files;
}


vector<string> readFileOrDirectory(const string& path) {
    vector<string> content;

    try {
        if (fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                content.push_back(entry.path().filename().string());
            }
            if (content.empty()) content.push_back("[Empty Directory]");
        } else {
            ifstream file(path);
            string line;
            if (file.is_open()) {
                while (getline(file, line)) content.push_back(line);
                file.close();
            } else {
                content.push_back("[Error: Cannot open file]");
            }
        }
    } catch (const fs::filesystem_error& e) {
        content.push_back("[Error: Permission Denied]");
    }

    return content;
}

void printFiles(WINDOW *win, const vector<string>& files, int startY, int startX, int selectedIndex, int scrollOffset) {
    werase(win);

    int height, width;
    getmaxyx(win, height, width);
    int maxItems = height - 2;

    for (size_t i = 0; i < maxItems && (i + scrollOffset) < files.size(); ++i) {
        string fileName = files[i + scrollOffset];
        string fullPath = currentPath + "/" + fileName;

        // Determine color based on file type
        int colorPair = 3; // Default: White (regular file)
        if (fs::is_directory(fullPath)) colorPair = 2;  // Blue for directories

        bool isMarked = markedFiles.count(fileName);  // Check if marked

        if ((i + scrollOffset) == selectedIndex) wattron(win, A_REVERSE);  // Highlight selection
        if (isMarked) wattron(win, A_UNDERLINE);  // Marked files in bold
        if (fs::is_directory(fullPath)) wattron(win, A_BOLD);  // Blue for directories

        wattron(win, COLOR_PAIR(colorPair));  // Apply color
        mvwprintw(win, startY + i, startX, "%c %s", isMarked ? 'M' : ' ', fileName.c_str());  // Show mark symbol

        wattroff(win, COLOR_PAIR(colorPair));
        if (isMarked) wattroff(win, A_UNDERLINE);
        if ((i + scrollOffset) == selectedIndex) wattroff(win, A_REVERSE);
		if (fs::is_directory(fullPath)) wattroff(win, A_BOLD);  // Blue for directories
    }

    wrefresh(win);
}

void printPreview(WINDOW *win, const vector<string>& content) {
    werase(win);

    int y = 1;
    for (size_t i = 0; i < content.size() && y < getmaxy(win) - 1; ++i, ++y) {
        mvwprintw(win, y, 1, "%s", content[i].c_str());
    }

    wrefresh(win);
}

void renameFileOrDirectory(WINDOW *prompt, vector<string>& files) {
    if (files.empty()) return;  // No files to rename

    string selectedItem = files[selectedIndex];
    string oldPath = currentPath + "/" + selectedItem;

    // Ask for new name
    werase(prompt);
    mvwprintw(prompt, 0, 1, "Rename '%s' to: ", selectedItem.c_str());
    wrefresh(prompt);

    char newName[256];
    echo();
    wgetnstr(prompt, newName, 255);
    noecho();

    if (string(newName).empty()) {
        mvwprintw(prompt, 0, 1, "Error: Name cannot be empty! ");
        wrefresh(prompt);
        return;
    }

    string newPath = currentPath + "/" + newName;

    if (fs::exists(newPath)) {
        mvwprintw(prompt, 0, 1, "Error: File already exists! ");
    } else {
        try {
            fs::rename(oldPath, newPath);
            mvwprintw(prompt, 0, 1, "Renamed successfully! ");
            files = getFiles(currentPath);  // Refresh file list
            selectedIndex = 0;
        } catch (const exception& e) {
            mvwprintw(prompt, 0, 1, "Error: %s", e.what());
        }
    }

    wrefresh(prompt);
}




int main() {
    initscr();
	start_color();
	use_default_colors();
	init_pair(1, COLOR_WHITE, -1);  // Regular files (white)
	init_pair(2, COLOR_BLUE, -1);   // Directories (blue)
	init_pair(3, COLOR_GREEN, -1);  // Executable files (green)

    cbreak();
    noecho();
    curs_set(0);

    int termHeight, termWidth;
    getmaxyx(stdscr, termHeight, termWidth);

	

	vector<string> files = getFiles(currentPath);

    WINDOW *leftPanel = newwin(termHeight - 3, termWidth / 2, 1, 0);
    WINDOW *rightPanel = newwin(termHeight - 3, termWidth / 2, 1, termWidth / 2);
    WINDOW *stackBar = newwin(1, termWidth, 0, 0);
    WINDOW *prompt = newwin(2, termWidth, termHeight - 3, 0);


	// Clear and refresh the prompt window
	werase(prompt);
	wattron(prompt, A_BOLD);
	mvwprintw(prompt, 1, 1, "%s", currentPath.c_str());  // Show current path
	wattroff(prompt, A_BOLD);

	// Display file permissions and last modified time

	wrefresh(prompt);


    keypad(leftPanel, TRUE);  // ✅ Fix: Enable special key input

	string currentUser = getUserName();
    vector<string> previewContent = readFileOrDirectory(currentPath + "/" + files[selectedIndex]);

    printFiles(leftPanel, files, 1, 1, selectedIndex, scrollOffset);
    printPreview(rightPanel, previewContent);

    int ch;
    while ((ch = wgetch(leftPanel)) != 'q') {
        refresh();


        int height, width;
        getmaxyx(leftPanel, height, width);
        int maxItems = height - 2;

        if (ch == KEY_UP || ch == 'k') {
            if (selectedIndex > 0) {
                selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset--;
            }
        }

        if (ch == KEY_DOWN || ch == 'j') {
            if (selectedIndex < (int)files.size() - 1) {
                selectedIndex++;
                if (selectedIndex >= scrollOffset + maxItems) scrollOffset++;
            }
        }

		if (ch == KEY_RIGHT || ch == 'l') {
			if (!files.empty()) {
				string selectedItem = files[selectedIndex];
				string fullPath = currentPath + "/" + selectedItem;

				if (fs::is_directory(fullPath)) { 
					currentPath = fullPath;
					files = getFiles(currentPath);  // Refresh file list
					selectedIndex = 0;
					scrollOffset = 0;
				} else {
					// Open file in editor
					endwin();
					string command = "nvim " + fullPath;
					system(command.c_str());
					initscr();
					cbreak();
					noecho();
					curs_set(0);
					keypad(leftPanel, TRUE);
				}

				// Update preview after changing directory or opening a file
				previewContent = readFileOrDirectory(currentPath + "/" + 
								(files.empty() ? "" : files[selectedIndex]));
				printFiles(leftPanel, files, 1, 1, selectedIndex, scrollOffset);
				printPreview(rightPanel, previewContent);
				werase(prompt);
				wattron(prompt, A_BOLD);
				mvwprintw(prompt, 1, 1, "%s", currentPath.c_str()); // Show updated path
				wattroff(prompt, A_BOLD);
				wrefresh(prompt);
			}
		}

        if (ch == KEY_LEFT || ch == 'h') {
            if (currentPath != "/") {
                string parentPath = fs::path(currentPath).parent_path().string();
                if (parentPath.empty()) parentPath = "/";
                
                currentPath = parentPath;
                files = getFiles(currentPath);
                selectedIndex = 0;
                scrollOffset = 0;

				werase(prompt);
				wattron(prompt, A_BOLD);
				mvwprintw(prompt, 1, 1, "%s", currentPath.c_str()); // Show updated path
				wattroff(prompt, A_BOLD);
				wrefresh(prompt);
            }
        }
		if (ch == 'G') {
			selectedIndex = files.size();
			scrollOffset = max(0, selectedIndex - maxItems + 1);
		}
		if (ch == 'g') {
			int d = wgetch(leftPanel);
			if (d == 'g') {
				selectedIndex = 0;
				scrollOffset = 0;
			} else {
				break;
			}
		}
		if (ch == 'n' || ch == 'N') {  // 'n' for file, 'N' for directory
			echo(); 
			char name[256];

			if (ch == 'n') {
				mvwprintw(prompt, 0, 1, "New file: ");
			} else {
				mvwprintw(prompt, 0, 1, "New directory: ");
			}

			wrefresh(prompt);
			wgetnstr(prompt, name, 255);
			noecho();

			string newPath = currentPath + "/" + name;

			if (fs::exists(newPath)) {  // ✅ Prevents crash if file/directory already exists
				mvwprintw(prompt, 0, 1, "Error: Already exists! ");
			} else {
				if (ch == 'n') {
					ofstream newFile(newPath);
					if (newFile) {
						newFile.close();
						mvwprintw(prompt, 0, 1, "File created!");
					} else {
						mvwprintw(prompt, 0, 1, "Error creating file!");
					}
				} else {
					if (fs::create_directory(newPath)) {
						mvwprintw(prompt, 0, 1, "Directory created!");
					} else {
						mvwprintw(prompt, 0, 1, "Error creating directory!");
					}
				}
			}

			files = getFiles(currentPath);
			wrefresh(prompt);
		}
		if (ch == 'r') {  // Detect F2 key
			renameFileOrDirectory(prompt, files);

			// Refresh file list and UI after renaming
			previewContent = readFileOrDirectory(currentPath + "/" + (files.empty() ? "" : files[selectedIndex]));
			printFiles(leftPanel, files, 1, 1, selectedIndex, scrollOffset);
			printPreview(rightPanel, previewContent);

			// Update the prompt with the new path
			werase(prompt);
			wattron(prompt, A_BOLD);
			mvwprintw(prompt, 1, 1, "%s", currentPath.c_str());
			wattroff(prompt, A_BOLD);
			wrefresh(prompt);
		}


		if (ch == 'd' && !files.empty()) {
			string selectedItem = files[selectedIndex];
			string filePath = currentPath + "/" + selectedItem;

			// Ask for confirmation
			mvwprintw(prompt, 0, 1, "Delete '%s'? (y/n): ", selectedItem.c_str());
			wrefresh(prompt);
			
			int confirm = wgetch(prompt); // Get user input

			if (confirm == 'y' || confirm == 'Y') {
				if (fs::remove_all(filePath)) {
					files = getFiles(currentPath); // Refresh file list
					mvwprintw(prompt, 0, 1, "Deleted successfully!          ");
				} else {
					mvwprintw(prompt, 0, 1, "Error deleting file!           ");
				}
			} else {
				mvwprintw(prompt, 0, 1, "Cancelled.                     ");
			}
			
			wrefresh(prompt);
		}
		if (ch == 32 && !files.empty()) {
			string selectedFile = files[selectedIndex];

			if (markedFiles.count(selectedFile)) {
				markedFiles.erase(selectedFile);  // Unmark if already marked
			} else {
				markedFiles.insert(selectedFile); // Mark if not already marked
			}
		}
		if (ch == 'D' && !markedFiles.empty()) {
			for (const auto& file : markedFiles) {
				string filePath = currentPath + "/" + file;

				mvwprintw(prompt, 0, 1, "Delete files? (y/n): ");
				wrefresh(prompt);
				
				int confirm = wgetch(prompt); // Get user input

				if (confirm == 'y' || confirm == 'Y') {
					if (fs::remove(filePath)) {
						files = getFiles(currentPath); // Refresh file list
						mvwprintw(prompt, 0, 1, "Deleted successfully!          ");
					} else {
						mvwprintw(prompt, 0, 1, "Error deleting file!           ");
					}
				} else {
					mvwprintw(prompt, 0, 1, "Cancelled.                     ");
				}
				
				wrefresh(prompt);
			}
			markedFiles.clear();  // Clear after deletion
			files = getFiles(currentPath);  // Refresh file list
		}
		if (ch == '/') {  // Enter filter mode
			filterQuery = "";
			werase(prompt);
			mvwprintw(prompt, 0, 0, "Filter: ");
			wrefresh(prompt);

			while (true) {
				int key = wgetch(prompt);

				if (key == 27) {  // ESC key: Exit filtering
					filterQuery = "";
					break;
				} else if (key == '\n') {  // Enter key: Confirm filter
					files = getFiles(currentPath, filterQuery);  // ✅ Apply filtering
					selectedIndex = 0;  // Reset selection
					scrollOffset = 0;
					break;
				} else if (key == KEY_BACKSPACE || key == 127) {  // Backspace
					if (!filterQuery.empty()) {
						filterQuery.pop_back();
					}
				} else if (isprint(key)) {  // Append valid characters
					filterQuery += (char)key;
				}

				// **Reprint UI**
				werase(prompt);
				mvwprintw(prompt, 0, 0, "Filter: %s", filterQuery.c_str());
				wrefresh(prompt);
				files = getFiles(currentPath, filterQuery);  // ✅ Refresh filtered files
				printFiles(leftPanel, files, 1, 1, selectedIndex, scrollOffset);
			}
		}








        if (selectedIndex >= (int)files.size()) selectedIndex = (int)files.size() - 1;
        if (selectedIndex < 0) selectedIndex = 0;



        previewContent = readFileOrDirectory(currentPath + "/" + (files.empty() ? "" : files[selectedIndex]));
        printFiles(leftPanel, files, 1, 1, selectedIndex, scrollOffset);
        printPreview(rightPanel, previewContent);

		werase(prompt);
		wattron(prompt, A_BOLD);
		mvwprintw(prompt, 1, 1, "%s", currentPath.c_str());  // Show current path
		wattroff(prompt, A_BOLD);

		// Display file permissions and last modified time

		wrefresh(prompt);
    }

    endwin();
    return 0;
}

