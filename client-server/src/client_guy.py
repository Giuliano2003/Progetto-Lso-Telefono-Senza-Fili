import socket
import threading
import tkinter as tk
from tkinter import simpledialog, scrolledtext, messagebox

SERVER_HOST = 'localhost'
SERVER_PORT = 8080

class ClientGUI:
    def __init__(self, master):
        self.master = master
        master.title("Client GUI TCP")

        self.chat_area = scrolledtext.ScrolledText(master, state='disabled', width=50, height=20)
        self.chat_area.pack(padx=10, pady=10)

        self.entry = tk.Entry(master, width=40)
        self.entry.pack(side='left', padx=(10, 0), pady=(0, 10))

        self.send_button = tk.Button(master, text="Send", command=self.send_message)
        self.send_button.pack(side='left', padx=(5, 10), pady=(0, 10))

        # ✅ Bottone "Crea Lobby"
        self.create_lobby_button = tk.Button(master, text="Crea Lobby", command=self.create_lobby)
        self.create_lobby_button.pack(side='left', padx=(5, 10), pady=(0, 10))

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        self.connect_to_server()

        self.running = True
        threading.Thread(target=self.receive_messages, daemon=True).start()

    def connect_to_server(self):
        try:
            self.sock.connect((SERVER_HOST, SERVER_PORT))
        except Exception as e:
            messagebox.showerror("Connection Error", str(e))
            self.master.quit()
            return

        username = simpledialog.askstring("Username", "Enter your username:")
        if username:
            self.sock.sendall(username.encode())
        else:
            self.master.quit()

    def send_message(self):
        message = self.entry.get()
        if message:
            self.sock.sendall(message.encode())
            self.entry.delete(0, tk.END)

    # ✅ Funzione associata a "Crea Lobby"
    def create_lobby(self):
        try:
            self.sock.sendall(b"100")  # Manda "100" come bytes
        except Exception as e:
            messagebox.showerror("Send Error", str(e))

    def receive_messages(self):
        while self.running:
            try:
                data = self.sock.recv(1024)
                if not data:
                    break
                message = data.decode()
                self.chat_area.configure(state='normal')
                self.chat_area.insert(tk.END, f"{message}\n")
                self.chat_area.configure(state='disabled')
                self.chat_area.yview(tk.END)
            except:
                break

        self.sock.close()

    def on_closing(self):
        self.running = False
        self.sock.close()
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    client = ClientGUI(root)
    root.protocol("WM_DELETE_WINDOW", client.on_closing)
    root.mainloop()
