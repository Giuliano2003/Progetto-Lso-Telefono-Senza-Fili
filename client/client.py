import socket
import threading
import tkinter as tk
from tkinter import simpledialog, messagebox, scrolledtext, ttk

SERVER_IP = '127.0.0.1'
SERVER_PORT = 8080

class ClientGUI:
    def __init__(self, master):
        self.master = master
        master.title("Telefono Senza Fili - Client")
        master.geometry("600x500")
        master.configure(bg="#f0f4f8")
        self.sock = None
        self.username = None

        # Font settings
        self.font_main = ("Segoe UI", 11)
        self.font_title = ("Segoe UI", 16, "bold")
        self.font_btn = ("Segoe UI", 11, "bold")

        # Title
        self.title_label = tk.Label(master, text="Telefono Senza Fili", font=self.font_title, bg="#f0f4f8", fg="#2d415a")
        self.title_label.pack(pady=(15, 5))

        # Separator
        ttk.Separator(master, orient='horizontal').pack(fill='x', padx=10, pady=5)

        # UI Elements
        self.text_area = scrolledtext.ScrolledText(master, height=12, width=65, state='disabled', font=self.font_main, bg="#f8fafc", fg="#222")
        self.text_area.pack(padx=15, pady=(5, 15))

        # Username frame
        self.user_frame = tk.Frame(master, bg="#f0f4f8")
        self.user_frame.pack(pady=(0, 10))

        self.entry = tk.Entry(self.user_frame, width=25, fg='grey', font=self.font_main, relief=tk.GROOVE, bd=2)
        self.entry.grid(row=0, column=0, padx=(0, 10))
        self.entry.insert(0, "Inserisci username...")
        self.entry.bind("<FocusIn>", self.clear_username_placeholder)
        self.entry.bind("<FocusOut>", self.restore_username_placeholder)

        self.login_btn = tk.Button(self.user_frame, text="Login", command=self.login, font=self.font_btn, bg="#4f8cff", fg="white", activebackground="#357ae8", width=10, relief=tk.RAISED, bd=2)
        self.login_btn.grid(row=0, column=1)

        # Lobby actions frame
        self.lobby_frame = tk.Frame(master, bg="#f0f4f8")
        self.lobby_frame.pack(pady=(0, 10))

        self.lobbies_btn = tk.Button(self.lobby_frame, text="Mostra Lobby", command=self.get_lobbies, state='disabled', font=self.font_btn, bg="#4f8cff", fg="white", activebackground="#357ae8", width=14, relief=tk.RAISED, bd=2)
        self.lobbies_btn.grid(row=0, column=0, padx=5)

        self.create_btn = tk.Button(self.lobby_frame, text="Crea Lobby", command=self.create_lobby, state='disabled', font=self.font_btn, bg="#4f8cff", fg="white", activebackground="#357ae8", width=14, relief=tk.RAISED, bd=2)
        self.create_btn.grid(row=0, column=1, padx=5)

        self.leave_btn = tk.Button(self.lobby_frame, text="Esci da Lobby", command=self.leave_lobby, state='disabled', font=self.font_btn, bg="#ff6b6b", fg="white", activebackground="#e84141", width=14, relief=tk.RAISED, bd=2)
        self.leave_btn.grid(row=0, column=2, padx=5)

        # Join lobby frame
        self.join_frame = tk.Frame(master, bg="#f0f4f8")
        self.join_frame.pack(pady=(0, 10))

        self.lobby_id_entry = tk.Entry(self.join_frame, width=32, fg='grey', font=self.font_main, relief=tk.GROOVE, bd=2)
        self.lobby_id_entry.grid(row=0, column=0, padx=(0, 10))
        self.lobby_id_entry.insert(0, "ID Lobby per entrare...")
        self.lobby_id_entry.bind("<FocusIn>", self.clear_lobbyid_placeholder)
        self.lobby_id_entry.bind("<FocusOut>", self.restore_lobbyid_placeholder)

        self.join_btn = tk.Button(self.join_frame, text="Entra in Lobby", command=self.join_lobby, state='disabled', font=self.font_btn, bg="#43d17a", fg="white", activebackground="#2fa75a", width=14, relief=tk.RAISED, bd=2)
        self.join_btn.grid(row=0, column=1)

        self.master.protocol("WM_DELETE_WINDOW", self.on_close)

        # Stato per feedback
        self.last_action = None

        # --- Campo invio manuale ---
        self.send_frame = tk.Frame(master, bg="#f0f4f8")
        self.send_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=10, pady=10)

        self.send_entry = tk.Entry(self.send_frame, width=45, font=self.font_main, relief=tk.GROOVE, bd=2)
        self.send_entry.pack(side=tk.LEFT, padx=(0, 10), ipady=2)
        self.send_entry.insert(0, "Scrivi richiesta TCP...")

        self.send_entry.bind("<FocusIn>", self.clear_send_placeholder)
        self.send_entry.bind("<FocusOut>", self.restore_send_placeholder)
        self.send_entry.bind("<Return>", lambda event: self.send_custom())

        self.send_btn = tk.Button(self.send_frame, text="Invia", command=self.send_custom, font=self.font_btn, bg="#222", fg="white", activebackground="#444", width=10, relief=tk.RAISED, bd=2)
        self.send_btn.pack(side=tk.LEFT)

    # Placeholder handlers
    def clear_username_placeholder(self, event):
        if self.entry.get() == "Inserisci username...":
            self.entry.delete(0, tk.END)
            self.entry.config(fg='black')

    def restore_username_placeholder(self, event):
        if not self.entry.get():
            self.entry.insert(0, "Inserisci username...")
            self.entry.config(fg='grey')

    def clear_lobbyid_placeholder(self, event):
        if self.lobby_id_entry.get() == "ID Lobby per entrare...":
            self.lobby_id_entry.delete(0, tk.END)
            self.lobby_id_entry.config(fg='black')

    def restore_lobbyid_placeholder(self, event):
        if not self.lobby_id_entry.get():
            self.lobby_id_entry.insert(0, "ID Lobby per entrare...")
            self.lobby_id_entry.config(fg='grey')

    def clear_send_placeholder(self, event):
        if self.send_entry.get() == "Scrivi richiesta TCP...":
            self.send_entry.delete(0, tk.END)
            self.send_entry.config(fg='black')

    def restore_send_placeholder(self, event):
        if not self.send_entry.get():
            self.send_entry.insert(0, "Scrivi richiesta TCP...")
            self.send_entry.config(fg='grey')

    def connect(self):
        if self.sock:
            return
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((SERVER_IP, SERVER_PORT))
        threading.Thread(target=self.listen_server, daemon=True).start()

    def listen_server(self):
        while True:
            try:
                data = self.sock.recv(1024)
                if not data:
                    break
                msg = data.decode()
                self.append_text(msg)
                self.handle_feedback(msg)
            except Exception as e:
                break

    def append_text(self, msg):
        self.text_area.config(state='normal')
        # Aggiungi una riga vuota tra i messaggi
        self.text_area.insert(tk.END, msg + "\n\n")
        self.text_area.see(tk.END)
        self.text_area.config(state='disabled')

    def handle_feedback(self, msg):
        # Analizza il codice di risposta e mostra feedback
        lines = msg.strip().split('\n')
        if not lines or not lines[0]:
            return
        code = lines[0]
        # Feedback in base all'ultima azione
        if code == "200":
            if self.last_action == "login":
                messagebox.showinfo("Login", "Login effettuato con successo!")
            elif self.last_action == "create_lobby":
                messagebox.showinfo("Lobby", "Lobby creata con successo!")
            elif self.last_action == "join_lobby":
                messagebox.showinfo("Lobby", "Entrato nella lobby!")
            elif self.last_action == "leave_lobby":
                messagebox.showinfo("Lobby", "Uscito dalla lobby!")
            elif self.last_action == "get_lobbies":
                # Mostra feedback anche se la lista è vuota
                if len(lines) > 1 and lines[1]:
                    messagebox.showinfo("Lobby", "Lista lobby aggiornata.")
                else:
                    messagebox.showinfo("Lobby", "Nessuna lobby disponibile.")
        elif code == "400":
            messagebox.showerror("Errore", "\n".join(lines[1:]))
        elif code == "404":
            messagebox.showerror("Errore", "Lobby non trovata.")
        elif code == "500":
            messagebox.showerror("Errore", "\n".join(lines[1:]))
        elif code == "300":
            messagebox.showwarning("Lobby", "L'host ha lasciato la lobby.")
        elif code == "301":
            messagebox.showwarning("Lobby", "Un giocatore ha lasciato la lobby.")
        # Reset azione
        self.last_action = None

    def login(self):
        username = self.entry.get().strip()
        if username == "Inserisci username...":
            messagebox.showerror("Errore", "Inserisci un username valido.")
            return
        if not (5 <= len(username) <= 15):
            messagebox.showerror("Errore", "Username deve essere tra 5 e 15 caratteri.")
            return
        self.connect()
        self.sock.sendall(f"200 {username}".encode())
        self.username = username
        self.lobbies_btn.config(state='normal')
        self.create_btn.config(state='normal')
        self.join_btn.config(state='normal')
        self.leave_btn.config(state='normal')
        self.login_btn.config(state='disabled')
        self.entry.config(state='disabled')
        self.last_action = "login"

    def get_lobbies(self):
        self.sock.sendall(b"102")
        self.last_action = "get_lobbies"

    def create_lobby(self):
        self.sock.sendall(b"100")
        self.last_action = "create_lobby"

    def join_lobby(self):
        lobby_id = self.lobby_id_entry.get().strip()
        if lobby_id == "ID Lobby per entrare...":
            messagebox.showerror("Errore", "Inserisci un ID lobby valido.")
            return
        if len(lobby_id) != 36:
            messagebox.showerror("Errore", "Inserisci un ID lobby valido (36 caratteri).")
            return
        self.sock.sendall(f"101 {lobby_id}".encode())
        self.last_action = "join_lobby"

    def leave_lobby(self):
        self.sock.sendall(b"103")
        self.last_action = "leave_lobby"

    def send_custom(self):
        msg = self.send_entry.get().strip()
        if not msg or msg == "Scrivi richiesta TCP...":
            return
        try:
            if self.sock:
                self.sock.sendall(msg.encode())
                self.append_text(f"[Inviato manualmente]: {msg}")
        except Exception as e:
            messagebox.showerror("Errore", f"Errore invio: {e}")
        self.send_entry.delete(0, tk.END)

    def on_close(self):
        try:
            if self.sock:
                self.sock.close()
        except:
            pass
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    gui = ClientGUI(root)
    root.mainloop()
