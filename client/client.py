import socket
import threading
import tkinter as tk
from tkinter import messagebox

HOST = 'localhost'
PORT = 8080

client_socket = None
username = ''
current_lobby_id = ''
is_host = False
refreshing = False  # To stop refresh thread when not on home

def connect_to_server():
    global client_socket
    try:
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((HOST, PORT))
    except Exception as e:
        messagebox.showerror("Connection Error", str(e))

def send_msg(msg):
    try:
        client_socket.sendall(msg.encode('utf-8'))
    except Exception as e:
        messagebox.showerror("Send Error", str(e))

def receive_lines():
    try:
        data = client_socket.recv(4096).decode('utf-8')
        return data.strip().split('\n')
    except Exception as e:
        messagebox.showerror("Receive Error", str(e))
        return []

class GameGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Socket Game")
        self.geometry("800x400")
        self.configure(bg='black')
        self.username = ''
        self.init_login_frame()

    def clear(self):
        global refreshing
        refreshing = False  # Stop background refresh
        for widget in self.winfo_children():
            widget.destroy()

    def init_login_frame(self):
        self.clear()
        frame = tk.Frame(self, bg="white", padx=20, pady=20)
        frame.place(relx=0.5, rely=0.5, anchor=tk.CENTER)

        tk.Label(frame, text="Username").pack()
        self.username_entry = tk.Entry(frame)
        self.username_entry.pack(pady=5)
        tk.Button(frame, text="Play!", command=self.login_action).pack(pady=5)

    def login_action(self):
        global username
        username = self.username_entry.get()
        if not username:
            return
        connect_to_server()
        send_msg(username)  # Send username as first message
        self.init_home_frame()


    def init_home_frame(self):
        global refreshing
        self.clear()
        refreshing = True

        tk.Label(self, text=f"Hello {username}!", fg='white', bg='black').pack(anchor='nw', padx=10, pady=10)

        self.lobby_frame = tk.Frame(self, bg="white")
        self.lobby_frame.pack(pady=10, padx=20)

        header = tk.Frame(self.lobby_frame, bg='white')
        header.pack(fill=tk.X)
        tk.Label(header, text="ID", width=40, anchor='w').pack(side=tk.LEFT)
        tk.Label(header, text="Host", width=20).pack(side=tk.LEFT)
        tk.Label(header, text="Players", width=10).pack(side=tk.LEFT)

        self.lobby_list = tk.Frame(self.lobby_frame, bg='white')
        self.lobby_list.pack()

        tk.Button(self, text="Create a lobby", command=self.create_lobby).pack(pady=10)

        self.fetch_lobbies()

        # Start auto-refresh
        threading.Thread(target=self.auto_refresh_lobbies, daemon=True).start()

    def fetch_lobbies(self):
        for widget in self.lobby_list.winfo_children():
            widget.destroy()
        send_msg("102")
        responses = receive_lines()
        for res in responses:
            parts = res.split()
            if len(parts) == 4:
                lobby_id, host, max_players, current_players = parts
                self.add_lobby_row(lobby_id, host, current_players, max_players)

    def auto_refresh_lobbies(self):
        while refreshing:
            self.fetch_lobbies()
            self.after(5000, lambda: None)  # Ensure UI thread is used
            threading.Event().wait(5)

    def add_lobby_row(self, lobby_id, host, current, max_):
        row = tk.Frame(self.lobby_list, bg='white')
        row.pack(fill=tk.X, pady=2)

        tk.Label(row, text=lobby_id, width=40, anchor='w').pack(side=tk.LEFT)
        tk.Label(row, text=host, width=20).pack(side=tk.LEFT)
        tk.Label(row, text=f"{current}/{max_}", width=10).pack(side=tk.LEFT)
        tk.Button(row, text="Join", command=lambda: self.join_lobby(lobby_id, host)).pack(side=tk.LEFT)

    def join_lobby(self, lobby_id, host_name):
        global current_lobby_id, is_host
        send_msg(f"101 {lobby_id}")
        current_lobby_id = lobby_id
        is_host = (host_name == username)
        self.init_lobby_frame()

    def create_lobby(self):
        global current_lobby_id, is_host
        send_msg("100")
        # Use a placeholder ID temporarily; in practice, you'd get this from server
        current_lobby_id = "new-host-lobby-id"
        is_host = True
        self.init_lobby_frame()

    def init_lobby_frame(self):
        self.clear()
        tk.Label(self, text=f"Lobby {current_lobby_id[:12]}...", bg="black", fg="white").pack(anchor='nw', padx=10, pady=10)

        chat_frame = tk.Frame(self, bg='white', bd=1, relief='sunken')
        chat_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=10)

        self.chat_display = tk.Text(chat_frame, state='disabled', height=10)
        self.chat_display.pack(fill=tk.BOTH, expand=True)

        bottom_frame = tk.Frame(self)
        bottom_frame.pack(fill=tk.X, padx=20, pady=5)

        self.msg_entry = tk.Entry(bottom_frame)
        self.msg_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)

        tk.Button(bottom_frame, text="Send").pack(side=tk.RIGHT, padx=5)

        btn_frame = tk.Frame(self)
        btn_frame.pack(anchor='ne', padx=20, pady=5)

        if is_host:
            tk.Button(btn_frame, text="Start the match!").pack(side=tk.RIGHT, padx=5)

        tk.Button(btn_frame, text="Leave", command=self.leave_lobby).pack(side=tk.RIGHT, padx=5)

    def leave_lobby(self):
        global current_lobby_id
        send_msg("103")
        current_lobby_id = ''
        self.init_home_frame()


if __name__ == '__main__':
    app = GameGUI()
    app.mainloop()
