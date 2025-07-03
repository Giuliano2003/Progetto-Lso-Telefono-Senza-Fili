import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import socket
import threading
import time

class GameClient:
    def __init__(self):
        print("[INIT] Starting GameClient")
        self.root = tk.Tk()
        self.root.title("Multilingual Word Game")
        self.root.geometry("800x600")
        self.root.configure(bg='#2c2c2c')
        self.socket = None
        self.connected = False
        self.authenticated = False
        self.player_name = ""
        self.current_lobby = None
        self.is_host = False
        self.lobbies = []
        self.receive_thread = None
        self.lobby_refresh_job = None
        self.main_frame = tk.Frame(self.root, bg='#2c2c2c')
        self.main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        self.connect_to_server()
        self.root.after(0, self.show_login_screen)

    def create_styled_button(self, parent, text, command, bg_color='#4a4a4a', fg_color='white', width=15):
        return tk.Button(parent, text=text, command=command, 
                       bg=bg_color, fg=fg_color, font=('Arial', 10, 'bold'),
                       relief=tk.FLAT, padx=10, pady=5, width=width,
                       activebackground='#5a5a5a', activeforeground='white')
    
    def create_styled_entry(self, parent, placeholder="", width=20):
        return tk.Entry(parent, font=('Arial', 10), width=width,
                        bg='white', fg='black', relief=tk.FLAT, bd=5)
    
    def create_styled_label(self, parent, text, font_size=12, color='white'):
        return tk.Label(parent, text=text, bg='#2c2c2c', fg=color,
                        font=('Arial', font_size))
    
    def clear_frame(self):
        if hasattr(self, 'lobby_refresh_job') and self.lobby_refresh_job:
            print("[UI] Cancelling lobby refresh job")
            self.root.after_cancel(self.lobby_refresh_job)
            self.lobby_refresh_job = None
        for widget in self.main_frame.winfo_children():
            widget.destroy()
    
    def connect_to_server(self):
        if self.connected:
            print("[NET] Already connected to server")
            return True
        try:
            print("[NET] Connecting to server...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect(('localhost', 8080))
            self.connected = True
            self.receive_thread = threading.Thread(target=self.receive_messages, daemon=True)
            self.receive_thread.start()
            print("[NET] Connected to server")
            return True
        except Exception as e:
            print(f"[ERROR] Failed to connect to server: {e}")
            messagebox.showerror("Connection Error", f"Failed to connect to server: {e}")
            return False
    
    def disconnect_from_server(self):
        if self.socket:
            try:
                print("[NET] Disconnecting from server")
                self.connected = False
                self.socket.close()
            except Exception as e:
                print(f"[ERROR] Exception during disconnect: {e}")
            self.socket = None

    def send_message(self, message):
        if self.socket and self.connected:
            try:
                print(f"[SEND] {message}")
                self.socket.send(message.encode())
                return True
            except Exception as e:
                print(f"[ERROR] Failed to send message: {e}")
                messagebox.showerror("Send Error", f"Failed to send message: {e}")
                self.disconnect_from_server()
                return False
        print("[WARN] Tried to send message while not connected")
        return False
    
    def receive_messages(self):
        print("[THREAD] Starting receive_messages thread")
        while self.connected:
            try:
                if self.socket:
                    message = self.socket.recv(4096).decode()
                    if message:
                        print(f"[RECV] {message.strip()}")
                        self.handle_server_message(message)
                    else:
                        print("[NET] Server closed connection")
                        break
            except Exception as e:
                if self.connected:
                    print(f"[ERROR] Receive error: {e}")
                break
        self.connected = False
        print("[THREAD] receive_messages thread exiting")
    
    def handle_server_message(self, message):
        lines = message.strip().split('\n')
        if not lines:
            print("[WARN] Received empty message from server")
            return

        status_code = lines[0]
        if self.in_lobby_window():
            msg_body = '\n'.join(lines[1:]).strip()
            if msg_body:
                self.print_lobby_message(msg_body)

        if status_code == "B02":
            print("[AUTH] Login successful")
            self.authenticated = True
            self.root.after(0, self.show_home_screen)
        elif status_code == "B01":
            print("[AUTH] Signup successful")
            messagebox.showinfo("Signup", "Signup successful! Please login.")
            self.root.after(0, self.show_login_screen)
        elif status_code == "A00":
            print("[LOBBY] Lobby created (host)")
            self.is_host = True
            if len(lines) > 1:
                self.current_lobby = lines[1].strip()
            else:
                self.current_lobby = ""
            self.root.after(0, self.show_lobby_host_screen)
        elif status_code == "A01":
            print("[LOBBY] Joined lobby (not host)")
            self.is_host = False
            self.root.after(0, self.show_lobby_screen)
        elif status_code == "A02":
            print("[LOBBY] Host left, lobby closed")
            self.current_lobby = None
            self.is_host = False
            self.root.after(0, self.show_home_screen)
        elif status_code == "A05":
            print("[LOBBY] Received lobbies list")
            self.parse_lobby_list('\n'.join(lines[1:]))
        elif status_code == "A10":
            print("[MATCH] Match started, wait for turn")
            self.root.after(0, self.show_not_your_turn_screen)
        elif status_code == "A11":
            print("[MATCH] Your turn")
            current_phrase = ""
            for l in lines:
                if l.startswith("The current phrase is:"):
                    current_phrase = l.split("The current phrase is:", 1)[1].strip()
                elif l.startswith("Start with a phrase"):
                    current_phrase = "Start with a phrase"
            self.root.after(0, lambda: self.show_your_turn_screen(current_phrase))
        elif status_code == "A13":
            print("[MATCH] Wait for others")
            self.root.after(0, self.show_not_your_turn_screen)
        elif status_code == "A12":
            print("[MATCH] Match terminated, show story")
            final_story = ""
            for idx, l in enumerate(lines):
                if l.startswith("Here is the story of the phrase:"):
                    final_story = '\n'.join(lines[idx+1:])
                    break
            self.root.after(0, lambda: self.show_match_end_screen(final_story))
        elif status_code == "A03":
            print("[MATCH] Switch to lobby screen (A03)")
            if self.in_match_window():
                if self.is_host:
                    self.root.after(0, self.show_lobby_host_screen)
                else:
                    self.root.after(0, self.show_lobby_screen)
        elif status_code == "A04" or status_code == "A07":
            print("[QUEUE] Added to queue for lobby")
            messagebox.showinfo("Queue", "You have been added to the queue for this lobby.")
        elif status_code == "Z01":
            error_msg = '\n'.join(lines[1:]) if len(lines) > 1 else "Bad request"
            print(f"[ERROR] Bad request: {error_msg}")
            self.root.after(0, lambda: messagebox.showerror("Error", error_msg))
        elif status_code == "Z02":
            error_msg = '\n'.join(lines[1:]) if len(lines) > 1 else "Conflict"
            print(f"[ERROR] Conflict: {error_msg}")
            self.root.after(0, lambda: messagebox.showerror("Error", error_msg))
        elif status_code == "Z03":
            error_msg = '\n'.join(lines[1:]) if len(lines) > 1 else "Unauthorized"
            print(f"[ERROR] Unauthorized: {error_msg}")
            self.root.after(0, lambda: messagebox.showerror("Error", error_msg))
        elif status_code == "Z00":
            error_msg = '\n'.join(lines[1:]) if len(lines) > 1 else "Server error"
            print(f"[ERROR] Server error: {error_msg}")
            self.root.after(0, lambda: messagebox.showerror("Error", error_msg))
        else:
            if message and not status_code.startswith('Z'):
                print("[LOBBY] Fallback: parsing as lobby list")
                self.parse_lobby_list(message)
    
    def parse_lobby_list(self, message):
        self.lobbies = []
        lines = message.strip().split('\n')
        for line in lines:
            if line.strip():
                parts = line.split()
                if len(parts) >= 4:
                    lobby_id = parts[0]
                    host_name = parts[1]
                    max_players = parts[2]
                    current_players = parts[3]
                    self.lobbies.append({
                        'id': lobby_id,
                        'host': host_name,
                        'players': f"{current_players}/{max_players}"
                    })
        print(f"[LOBBY] Parsed {len(self.lobbies)} lobbies")
        self.root.after(0, self.refresh_lobby_list)
    
    def show_login_screen(self):
        self.clear_frame()
        login_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        login_frame.pack(expand=True)
        title = self.create_styled_label(login_frame, "Login", 16, 'white')
        title.pack(pady=20)
        self.create_styled_label(login_frame, "Username").pack(pady=5)
        self.username_entry = self.create_styled_entry(login_frame, width=30)
        self.username_entry.pack(pady=5)
        self.create_styled_label(login_frame, "Password").pack(pady=5)
        self.password_entry = self.create_styled_entry(login_frame, width=30)
        self.password_entry.config(show="*")
        self.password_entry.pack(pady=5)
        btn_frame = tk.Frame(login_frame, bg='#2c2c2c')
        btn_frame.pack(pady=20)
        self.create_styled_button(btn_frame, "Play!", self.login).pack(side=tk.LEFT, padx=5)
        self.create_styled_button(btn_frame, "Sign Up", self.show_signup_screen, bg_color='#6a6a6a').pack(side=tk.LEFT, padx=5)
    
    def show_signup_screen(self):
        self.clear_frame()
        signup_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        signup_frame.pack(expand=True)
        title = self.create_styled_label(signup_frame, "Sign Up", 16, 'white')
        title.pack(pady=20)
        self.create_styled_label(signup_frame, "Username").pack(pady=5)
        self.signup_username_entry = self.create_styled_entry(signup_frame, width=30)
        self.signup_username_entry.pack(pady=5)
        self.create_styled_label(signup_frame, "Password").pack(pady=5)
        self.signup_password_entry = self.create_styled_entry(signup_frame, width=30)
        self.signup_password_entry.config(show="*")
        self.signup_password_entry.pack(pady=5)
        self.create_styled_label(signup_frame, "Language").pack(pady=5)
        self.signup_language_var = tk.StringVar(value="en")
        lang_frame = tk.Frame(signup_frame, bg='#2c2c2c')
        lang_frame.pack(pady=5)
        languages = [("English", "en"), ("Italian", "it"), ("Spanish", "es"), ("French", "fr"), ("German", "de")]
        for text, code in languages:
            tk.Radiobutton(lang_frame, text=text, variable=self.signup_language_var, value=code,
                          bg='#2c2c2c', fg='white', selectcolor='#4a4a4a',
                          activebackground='#2c2c2c', activeforeground='white').pack(side=tk.LEFT, padx=5)
        btn_frame = tk.Frame(signup_frame, bg='#2c2c2c')
        btn_frame.pack(pady=20)
        self.create_styled_button(btn_frame, "Sign Up", self.signup).pack(side=tk.LEFT, padx=5)
        self.create_styled_button(btn_frame, "Back to Login", self.show_login_screen, bg_color='#6a6a6a').pack(side=tk.LEFT, padx=5)
    
    def login(self):
        if not self.connected:
            if not self.connect_to_server():
                return
        username = self.username_entry.get().strip()
        password = self.password_entry.get().strip()
        if not username or not password:
            print("[ERROR] Username or password missing in login")
            messagebox.showerror("Error", "Please enter username and password")
            return
        self.player_name = username
        message = f"202 {username} {password}"
        self.send_message(message)
    
    def signup(self):
        if not self.connected:
            if not self.connect_to_server():
                return
        username = self.signup_username_entry.get().strip()
        password = self.signup_password_entry.get().strip()
        language = self.signup_language_var.get()
        if not username or not password:
            print("[ERROR] Username or password missing in signup")
            messagebox.showerror("Error", "Please enter username and password")
            return
        message = f"201 {language} {username} {password}"
        self.send_message(message)
    
    def show_home_screen(self):
        print("[UI] Showing home screen")
        self.clear_frame()
        self.authenticated = True
        if hasattr(self, 'lobby_refresh_job') and self.lobby_refresh_job:
            self.root.after_cancel(self.lobby_refresh_job)
            self.lobby_refresh_job = None
        header_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        header_frame.pack(fill=tk.X, pady=10)
        welcome_label = self.create_styled_label(header_frame, f"Hello {self.player_name}", 16, 'white')
        welcome_label.pack(side=tk.LEFT)
        self.create_styled_button(header_frame, "Create a lobby", self.create_lobby).pack(side=tk.RIGHT)
        lobby_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        lobby_frame.pack(fill=tk.BOTH, expand=True)
        headers_frame = tk.Frame(lobby_frame, bg='#4a4a4a')
        headers_frame.pack(fill=tk.X, pady=5)
        self.create_styled_label(headers_frame, "ID", 10, 'white').pack(side=tk.LEFT, padx=10)
        self.create_styled_label(headers_frame, "Host", 10, 'white').pack(side=tk.LEFT, padx=50)
        self.create_styled_label(headers_frame, "Players", 10, 'white').pack(side=tk.LEFT, padx=50)
        self.lobby_list_frame = tk.Frame(lobby_frame, bg='#2c2c2c')
        self.lobby_list_frame.pack(fill=tk.BOTH, expand=True)
        self.send_message("102")
        self.schedule_lobby_refresh()

    def schedule_lobby_refresh(self):
        def refresh():
            if hasattr(self, 'lobby_list_frame') and self.lobby_list_frame.winfo_exists():
                print("[UI] Refreshing lobby list")
                self.send_message("102")
                self.lobby_refresh_job = self.root.after(5000, refresh)  # changed from 10000 to 5000
            else:
                self.lobby_refresh_job = None
        self.lobby_refresh_job = self.root.after(5000, refresh)  # changed from 10000 to 5000

    def refresh_lobby_list(self):
        if not hasattr(self, 'lobby_list_frame'):
            print("[WARN] No lobby_list_frame to refresh")
            return
        try:
            if not self.lobby_list_frame.winfo_exists():
                print("[WARN] lobby_list_frame does not exist")
                return
        except tk.TclError:
            print("[ERROR] TclError in refresh_lobby_list")
            return
        for widget in self.lobby_list_frame.winfo_children():
            widget.destroy()
        for lobby in self.lobbies:
            lobby_row = tk.Frame(self.lobby_list_frame, bg='#3a3a3a')
            lobby_row.pack(fill=tk.X, pady=2, padx=5)
            display_id = lobby['id'][:20] + "..." if len(lobby['id']) > 20 else lobby['id']
            self.create_styled_label(lobby_row, display_id, 10, 'white').pack(side=tk.LEFT, padx=10)
            self.create_styled_label(lobby_row, lobby['host'], 10, 'white').pack(side=tk.LEFT, padx=50)
            self.create_styled_label(lobby_row, lobby['players'], 10, 'white').pack(side=tk.LEFT, padx=50)
            join_btn = self.create_styled_button(lobby_row, "Join", 
                                               lambda lid=lobby['id']: self.join_lobby(lid), 
                                               bg_color='#5a5a5a', width=8)
            join_btn.pack(side=tk.RIGHT, padx=10)
    
    def create_lobby(self):
        if not self.authenticated:
            print("[ERROR] Tried to create lobby while not authenticated")
            messagebox.showerror("Error", "You must be logged in to create a lobby.")
            return
        print("[LOBBY] Creating lobby")
        message = "100"
        self.send_message(message)
        self.is_host = True
        self.current_lobby = ""
        self.show_lobby_host_screen()
    
    def join_lobby(self, lobby_id):
        print(f"[LOBBY] Joining lobby {lobby_id}")
        self.current_lobby = lobby_id
        message = f"101 {lobby_id}"
        self.send_message(message)
        self.show_lobby_screen()
    
    def show_lobby_host_screen(self):
        print("[UI] Showing lobby host screen")
        self.clear_frame()
        header_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        header_frame.pack(fill=tk.X, pady=10)
        lobby_title = self.current_lobby[:20] + "..." if self.current_lobby and len(self.current_lobby) > 20 else self.current_lobby
        title_label = self.create_styled_label(header_frame, f"Lobby {lobby_title}", 14, 'white')
        title_label.pack(side=tk.LEFT)
        btn_frame = tk.Frame(header_frame, bg='#2c2c2c')
        btn_frame.pack(side=tk.RIGHT)
        self.create_styled_button(btn_frame, "Leave", self.leave_lobby, bg_color='#d32f2f').pack(side=tk.RIGHT, padx=5)
        self.create_styled_button(btn_frame, "Start the match!", self.start_match).pack(side=tk.RIGHT, padx=5)
        chat_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        chat_frame.pack(fill=tk.BOTH, expand=True, pady=10)
        self.chat_display = scrolledtext.ScrolledText(chat_frame, bg='white', fg='black',
                                                     font=('Arial', 10), state=tk.DISABLED, height=15)
        self.chat_display.pack(fill=tk.BOTH, expand=True)

    def show_lobby_screen(self):
        print("[UI] Showing lobby screen")
        self.clear_frame()
        header_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        header_frame.pack(fill=tk.X, pady=10)
        lobby_title = self.current_lobby[:20] + "..." if self.current_lobby and len(self.current_lobby) > 20 else self.current_lobby
        title_label = self.create_styled_label(header_frame, f"Lobby {lobby_title}", 14, 'white')
        title_label.pack(side=tk.LEFT)
        self.create_styled_button(header_frame, "Leave", self.leave_lobby, bg_color='#d32f2f').pack(side=tk.RIGHT)
        chat_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        chat_frame.pack(fill=tk.BOTH, expand=True, pady=10)
        self.chat_display = scrolledtext.ScrolledText(chat_frame, bg='white', fg='black',
                                                     font=('Arial', 10), state=tk.DISABLED, height=15)
        self.chat_display.pack(fill=tk.BOTH, expand=True)
    
    def show_your_turn_screen(self, current_phrase):
        print("[UI] Showing your turn screen")
        self.clear_frame()
        header_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        header_frame.pack(fill=tk.X, pady=10)
        lobby_title = self.current_lobby[:20] + "..." if self.current_lobby and len(self.current_lobby) > 20 else self.current_lobby
        title_label = self.create_styled_label(header_frame, f"Lobby {lobby_title}", 14, 'white')
        title_label.pack(side=tk.LEFT)
        self.create_styled_button(header_frame, "Leave", self.leave_lobby, bg_color='#d32f2f').pack(side=tk.RIGHT)
        game_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        game_frame.pack(fill=tk.BOTH, expand=True, pady=20)
        phrase_frame = tk.Frame(game_frame, bg='#4a4a4a', relief=tk.RAISED, bd=2)
        phrase_frame.pack(fill=tk.X, pady=10, padx=20)
        if current_phrase == "Start with a phrase":
            self.create_styled_label(phrase_frame, current_phrase, 14, '#ffd700').pack(pady=10)
        else:
            self.create_styled_label(phrase_frame, "The current phrase is:", 12, 'white').pack(pady=5)
            phrase_label = self.create_styled_label(phrase_frame, current_phrase, 14, '#ffd700')
            phrase_label.pack(pady=10)
        input_frame = tk.Frame(game_frame, bg='#2c2c2c')
        input_frame.pack(pady=20)
        self.phrase_entry = self.create_styled_entry(input_frame, width=40)
        self.phrase_entry.pack(side=tk.LEFT, padx=5)
        self.create_styled_button(input_frame, "Send", self.send_phrase).pack(side=tk.LEFT, padx=5)

    def show_not_your_turn_screen(self):
        print("[UI] Showing not your turn screen")
        self.clear_frame()
        header_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        header_frame.pack(fill=tk.X, pady=10)
        lobby_title = self.current_lobby[:20] + "..." if self.current_lobby and len(self.current_lobby) > 20 else self.current_lobby
        title_label = self.create_styled_label(header_frame, f"Lobby {lobby_title}", 14, 'white')
        title_label.pack(side=tk.LEFT)
        self.create_styled_button(header_frame, "Leave", self.leave_lobby, bg_color='#d32f2f').pack(side=tk.RIGHT)
        wait_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        wait_frame.pack(expand=True)
        self.create_styled_label(wait_frame, "Wait for the other players to finish their turn", 14, 'white').pack()

    def show_match_end_screen(self, final_story):
        print("[UI] Showing match end screen")
        self.clear_frame()
        header_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        header_frame.pack(fill=tk.X, pady=10)
        lobby_title = self.current_lobby[:20] + "..." if self.current_lobby and len(self.current_lobby) > 20 else self.current_lobby
        title_label = self.create_styled_label(header_frame, f"Lobby {lobby_title}", 14, 'white')
        title_label.pack(side=tk.LEFT)
        self.create_styled_button(header_frame, "Leave", self.leave_lobby, bg_color='#d32f2f').pack(side=tk.RIGHT)
        results_frame = tk.Frame(self.main_frame, bg='#2c2c2c')
        results_frame.pack(fill=tk.BOTH, expand=True, pady=20, padx=20)
        self.create_styled_label(results_frame, "Match Completed!", 16, '#ffd700').pack(pady=10)
        self.create_styled_label(results_frame, "Here is the story of the phrase:", 12, 'white').pack(pady=5)

        # --- Extract the final phrase after '=>'
        story_part = final_story
        final_phrase = ""
        if "=>" in final_story:
            story_part, final_phrase = final_story.rsplit("=>", 1)
            final_phrase = final_phrase.strip()
            story_part = story_part.strip()
        # ---

        # Enhanced story timeline, splitting by '->'
        timeline_frame = tk.Frame(results_frame, bg='#2c2c2c')
        timeline_frame.pack(fill=tk.BOTH, expand=True, pady=10)
        # Split by '->', trim whitespace, skip empty
        steps = [part.strip() for part in story_part.strip().split('->') if part.strip()]
        colors = ['#f5f5f5', '#e3f2fd']
        border_colors = ['#ffd700', '#2196f3']
        for idx, phrase in enumerate(steps):
            step_frame = tk.Frame(
                timeline_frame,
                bg=colors[idx % 2],
                highlightbackground=border_colors[idx % 2],
                highlightthickness=2,
                bd=0,
                relief=tk.RIDGE
            )
            step_frame.pack(fill=tk.X, pady=6, padx=10, anchor='w')
            # Numbered step
            step_label = tk.Label(
                step_frame,
                text=f"Step {idx+1}",
                bg=colors[idx % 2],
                fg='#333',
                font=('Arial', 10, 'bold')
            )
            step_label.pack(side=tk.LEFT, padx=(10, 15), pady=8)
            # Phrase text
            phrase_label = tk.Label(
                step_frame,
                text=phrase,
                bg=colors[idx % 2],
                fg='#222',
                font=('Arial', 12, 'italic'),
                wraplength=600,
                justify=tk.LEFT
            )
            phrase_label.pack(side=tk.LEFT, padx=5, pady=8)

        # --- Show the final phrase as a box like the other steps, but labeled "Final phrase"
        if final_phrase:
            final_frame = tk.Frame(
                timeline_frame,
                bg='#e8f5e9',
                highlightbackground='#4caf50',
                highlightthickness=2,
                bd=0,
                relief=tk.RIDGE
            )
            final_frame.pack(fill=tk.X, pady=10, padx=10, anchor='w')
            final_label = tk.Label(
                final_frame,
                text="Final phrase",
                bg='#e8f5e9',
                fg='#388e3c',
                font=('Arial', 10, 'bold')
            )
            final_label.pack(side=tk.LEFT, padx=(10, 15), pady=8)
            phrase_label = tk.Label(
                final_frame,
                text=final_phrase,
                bg='#e8f5e9',
                fg='#222',
                font=('Arial', 12, 'italic'),
                wraplength=600,
                justify=tk.LEFT
            )
            phrase_label.pack(side=tk.LEFT, padx=5, pady=8)
        # ---

        if self.is_host:
            btn_frame = tk.Frame(results_frame, bg='#2c2c2c')
            btn_frame.pack(pady=10)
            # Use lambda to force clockwise direction on restart
            self.create_styled_button(btn_frame, "Start a new match", lambda: self.start_match(force_clockwise=True)).pack(side=tk.LEFT, padx=5)

    def add_chat_message(self, username, message):
        if hasattr(self, 'chat_display'):
            self.chat_display.config(state=tk.NORMAL)
            self.chat_display.insert(tk.END, f"{username}: {message}\n")
            self.chat_display.config(state=tk.DISABLED)
            self.chat_display.see(tk.END)
    
    def send_chat_message(self):
        if hasattr(self, 'message_entry'):
            message = self.message_entry.get().strip()
            if message:
                self.add_chat_message(self.player_name, message)
                self.message_entry.delete(0, tk.END)
    
    def send_phrase(self):
        if hasattr(self, 'phrase_entry'):
            phrase = self.phrase_entry.get().strip()
            if phrase:
                length = len(phrase)
                if length >= 100:
                    print("[ERROR] Phrase too long (max 99 characters)")
                    messagebox.showerror("Error", "Phrase too long (max 99 characters)")
                    return
                message = f"111 {length:02d} {phrase}"
                print(f"[SEND] Phrase: {phrase}")
                self.send_message(message)
                self.phrase_entry.delete(0, tk.END)
    
    def start_match(self, force_clockwise=False):
        print("[MATCH] Host starting match")
        def send_with_direction(direction):
            message = f"110 {direction}"
            print(f"[MATCH] Sending start match with direction {direction}")
            self.send_message(message)
        if not self.is_host:
            print("[ERROR] Only the host can start the match.")
            messagebox.showerror("Error", "Only the host can start the match.")
            return
        # If forced, always send clockwise and skip dialog
        if force_clockwise:
            send_with_direction(1)
            return
        direction_dialog = tk.Toplevel(self.root)
        direction_dialog.title("Choose Match Direction")
        direction_dialog.geometry("300x150")
        direction_dialog.configure(bg='#2c2c2c')
        direction_dialog.grab_set()
        direction_dialog.transient(self.root)
        label = self.create_styled_label(direction_dialog, "Choose the direction of the match:", 12, 'white')
        label.pack(pady=20)
        btn_frame = tk.Frame(direction_dialog, bg='#2c2c2c')
        btn_frame.pack(pady=10)
        def choose_clockwise():
            send_with_direction(1)
            direction_dialog.destroy()
        def choose_counter():
            send_with_direction(0)
            direction_dialog.destroy()
        self.create_styled_button(btn_frame, "Clockwise", choose_clockwise, bg_color='#4caf50').pack(side=tk.LEFT, padx=10)
        self.create_styled_button(btn_frame, "Counter-Clockwise", choose_counter, bg_color='#2196f3').pack(side=tk.LEFT, padx=10)

    def leave_lobby(self):
        print("[LOBBY] Leaving lobby")
        message = "103"
        self.send_message(message)
        self.root.after(0, self.show_home_screen)
    
    def on_closing(self):
        print("[UI] Closing application")
        self.disconnect_from_server()
        self.root.destroy()
    
    def run(self):
        print("[MAIN] Running mainloop")
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.root.mainloop()

    def in_lobby_window(self):
        return hasattr(self, 'chat_display') and self.chat_display.winfo_exists()

    def in_match_window(self):
        for widget in self.main_frame.winfo_children():
            if hasattr(self, 'phrase_entry') and self.phrase_entry.winfo_exists():
                return True
            if isinstance(widget, tk.Frame):
                for child in widget.winfo_children():
                    if isinstance(child, tk.Label) and "Wait for the other players" in child.cget("text"):
                        return True
            if hasattr(self, 'story_display') and self.story_display.winfo_exists():
                return True
        return False

    def print_lobby_message(self, msg):
        if hasattr(self, 'chat_display'):
            self.chat_display.config(state=tk.NORMAL)
            self.chat_display.insert(tk.END, f"{msg}\n")
            start_idx = self.chat_display.index("end-1l linestart")
            end_idx = self.chat_display.index("end-1l lineend")
            self.chat_display.tag_add("server", start_idx, end_idx)
            self.chat_display.tag_config("server", foreground="#888888", font=('Arial', 10, 'italic'))
            self.chat_display.config(state=tk.DISABLED)
            self.chat_display.see(tk.END)

if __name__ == "__main__":
    client = GameClient()
    client.run()