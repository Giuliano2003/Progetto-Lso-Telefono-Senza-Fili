import socket
import threading
import tkinter as tk
from tkinter import simpledialog, messagebox, scrolledtext, ttk

SERVER_IP = '127.0.0.1'
SERVER_PORT = 8080

LANGUAGES = {
    "Inglese": "en",
    "Spagnolo": "es",
    "Francese": "fr",
    "Italiano": "it",
    "Tedesco": "de"
}

class ClientGUI:
    def __init__(self, master):
        self.master = master
        master.title("Telefono Senza Fili - Client")
        master.geometry("700x600")
        master.configure(bg="#f0f4f8")
        self.sock = None
        self.username = None
        self.is_host = False
        self.in_lobby = False

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

        # Language selection combobox
        self.lang_var = tk.StringVar(value="Italiano")
        self.lang_combo = ttk.Combobox(self.user_frame, textvariable=self.lang_var, state="readonly", width=12, font=self.font_main)
        self.lang_combo['values'] = list(LANGUAGES.keys())
        self.lang_combo.grid(row=0, column=0, padx=(0, 10))

        self.entry = tk.Entry(self.user_frame, width=15, fg='grey', font=self.font_main, relief=tk.GROOVE, bd=2)
        self.entry.grid(row=0, column=1, padx=(0, 10))
        self.entry.insert(0, "Username...")
        self.entry.bind("<FocusIn>", self.clear_username_placeholder)
        self.entry.bind("<FocusOut>", self.restore_username_placeholder)

        self.pw_entry = tk.Entry(self.user_frame, width=15, fg='grey', font=self.font_main, relief=tk.GROOVE, bd=2, show="*")
        self.pw_entry.grid(row=0, column=2, padx=(0, 10))
        self.pw_entry.insert(0, "Password...")
        self.pw_entry.bind("<FocusIn>", self.clear_pw_placeholder)
        self.pw_entry.bind("<FocusOut>", self.restore_pw_placeholder)

        self.signup_btn = tk.Button(self.user_frame, text="Signup", command=self.signup, font=self.font_btn, bg="#43d17a", fg="white", activebackground="#2fa75a", width=10, relief=tk.RAISED, bd=2)
        self.signup_btn.grid(row=0, column=3, padx=(0, 5))

        self.login_btn = tk.Button(self.user_frame, text="Login", command=self.login, font=self.font_btn, bg="#4f8cff", fg="white", activebackground="#357ae8", width=10, relief=tk.RAISED, bd=2)
        self.login_btn.grid(row=0, column=4)

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

        # --- Pulsante per startare il match (solo host) ---
        self.start_frame = tk.Frame(master, bg="#f0f4f8")
        # Sposta subito sopra la barra invio manuale
        self.start_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=10, pady=(0, 2))
        self.start_btn = tk.Button(self.start_frame, text="Start Match", command=self.start_match, state='disabled', font=self.font_btn, bg="#ffb347", fg="white", activebackground="#e89c1a", width=14, relief=tk.RAISED, bd=2)
        self.start_btn.pack(side=tk.LEFT)

        # --- Barra invio frase per il turno ---
        self.turn_frame = tk.Frame(master, bg="#f0f4f8")
        self.turn_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=10, pady=(0, 2))
        self.turn_entry = tk.Entry(self.turn_frame, width=45, font=self.font_main, relief=tk.GROOVE, bd=2, state='disabled')
        self.turn_entry.pack(side=tk.LEFT, padx=(0, 10), ipady=2)
        self.turn_entry.insert(0, "Scrivi la tua frase...")
        self.turn_entry.bind("<FocusIn>", self.clear_turn_placeholder)
        self.turn_entry.bind("<FocusOut>", self.restore_turn_placeholder)
        self.turn_entry.bind("<Return>", lambda event: self.send_turn_phrase())
        self.turn_send_btn = tk.Button(self.turn_frame, text="Invia Frase", command=self.send_turn_phrase, font=self.font_btn, bg="#43d17a", fg="white", activebackground="#2fa75a", width=12, relief=tk.RAISED, bd=2, state='disabled')
        self.turn_send_btn.pack(side=tk.LEFT)

    # Placeholder handlers
    def clear_username_placeholder(self, event):
        if self.entry.get() == "Username...":
            self.entry.delete(0, tk.END)
            self.entry.config(fg='black')

    def restore_username_placeholder(self, event):
        if not self.entry.get():
            self.entry.insert(0, "Username...")
            self.entry.config(fg='grey')

    def clear_pw_placeholder(self, event):
        if self.pw_entry.get() == "Password...":
            self.pw_entry.delete(0, tk.END)
            self.pw_entry.config(fg='black', show="*")

    def restore_pw_placeholder(self, event):
        if not self.pw_entry.get():
            self.pw_entry.insert(0, "Password...")
            self.pw_entry.config(fg='grey', show="*")

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

    def clear_turn_placeholder(self, event):
        if self.turn_entry.get() == "Scrivi la tua frase...":
            self.turn_entry.delete(0, tk.END)
            self.turn_entry.config(fg='black')

    def restore_turn_placeholder(self, event):
        if not self.turn_entry.get():
            self.turn_entry.insert(0, "Scrivi la tua frase...")
            self.turn_entry.config(fg='grey')

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
            if self.last_action == "signup":
                messagebox.showinfo("Signup", "Registrazione effettuata con successo! Ora puoi fare login.")
            elif self.last_action == "login":
                messagebox.showinfo("Login", "Login effettuato con successo!")
                self.lobbies_btn.config(state='normal')
                self.create_btn.config(state='normal')
                self.join_btn.config(state='normal')
                self.leave_btn.config(state='normal')
                self.login_btn.config(state='disabled')
                self.signup_btn.config(state='disabled')
                self.entry.config(state='disabled')
                self.pw_entry.config(state='disabled')
                self.lang_combo.config(state='disabled')
            elif self.last_action == "create_lobby":
                messagebox.showinfo("Lobby", "Lobby creata con successo!")
                self.create_btn.config(state='disabled')
                self.join_btn.config(state='disabled')
            elif self.last_action == "join_lobby":
                messagebox.showinfo("Lobby", "Entrato nella lobby!")
                self.create_btn.config(state='disabled')
                self.join_btn.config(state='disabled')
            elif self.last_action == "leave_lobby":
                messagebox.showinfo("Lobby", "Uscito dalla lobby!")
                self.create_btn.config(state='normal')
                self.join_btn.config(state='normal')
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
            # Non disabilitare i tasti in caso di errore 404
        elif code == "500":
            messagebox.showerror("Errore", "\n".join(lines[1:]))
        elif code == "300":
            messagebox.showwarning("Lobby", "L'host ha lasciato la lobby.")
        elif code == "301":
            messagebox.showwarning("Lobby", "Un giocatore ha lasciato la lobby.")
        elif code == "302":
            messagebox.showinfo("Lobby", "Is your turn!")
            self.enable_turn_entry()  # Abilita sempre la entry/bottone quando è il proprio turno
        elif code == "303":
            messagebox.showinfo("Lobby", "Wait for the other players to finish")
            self.disable_turn_entry()  # Disabilita la entry se non è il proprio turno
        elif code == "304":
            messagebox.showinfo("Lobby", "The match is terminated.")
            self.disable_turn_entry()  # Disabilita la entry a fine partita
            # Riabilita il tasto start match se host e in lobby
            if self.is_host and self.in_lobby:
                self.start_btn.config(state='normal')
        # Reset azione
        self.last_action = None

        # Gestione messaggi di turno
        if code == "300":
            # Cerca se è il proprio turno
            for line in lines:
                if "Is your turn!" in line:
                    self.enable_turn_entry()
                    break
            else:
                self.disable_turn_entry()

    def signup(self):
        username = self.entry.get().strip()
        password = self.pw_entry.get().strip()
        if username == "Username..." or not username:
            messagebox.showerror("Errore", "Inserisci un username valido.")
            return
        if password == "Password..." or not password:
            messagebox.showerror("Errore", "Inserisci una password valida.")
            return
        if not (5 <= len(username) <= 15):
            messagebox.showerror("Errore", "Username deve essere tra 5 e 15 caratteri.")
            return
        lang_name = self.lang_var.get()
        lang_code = LANGUAGES.get(lang_name, "it")
        self.connect()
        self.sock.sendall(f"201 {lang_code} {username} {password}".encode())
        self.last_action = "signup"

    def login(self):
        username = self.entry.get().strip()
        password = self.pw_entry.get().strip()
        if username == "Username..." or not username:
            messagebox.showerror("Errore", "Inserisci un username valido.")
            return
        if password == "Password..." or not password:
            messagebox.showerror("Errore", "Inserisci una password valida.")
            return
        self.connect()
        self.sock.sendall(f"202 {username} {password}".encode())
        self.username = username
        self.last_action = "login"

    def get_lobbies(self):
        self.sock.sendall(b"102")
        self.last_action = "get_lobbies"

    def create_lobby(self):
        self.sock.sendall(b"100")
        self.last_action = "create_lobby"
        self.is_host = True
        self.in_lobby = True
        self.update_start_btn_state()
        self.create_btn.config(state='disabled')  # Disattiva dopo creazione
        self.join_btn.config(state='disabled')    # Disattiva dopo creazione

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
        # Disabilita i tasti solo dopo conferma dal server (in handle_feedback)

    def leave_lobby(self):
        self.sock.sendall(b"103")
        self.last_action = "leave_lobby"
        self.is_host = False
        self.in_lobby = False
        self.update_start_btn_state()
        self.disable_turn_entry()
        self.create_btn.config(state='normal')    # Riattiva dopo uscita
        self.join_btn.config(state='normal')      # Riattiva dopo uscita

    def start_match(self):
        # Chiedi direzione (clockwise/counterclockwise)
        direction = messagebox.askquestion("Direzione", "Vuoi giocare in senso orario?", icon='question')
        clockwise = "1" if direction == "yes" else "0"
        self.sock.sendall(f"110 {clockwise}".encode())
        self.start_btn.config(state='disabled')

    def enable_turn_entry(self):
        self.turn_entry.config(state='normal')
        self.turn_send_btn.config(state='normal')
        self.turn_entry.delete(0, tk.END)
        self.turn_entry.insert(0, "Scrivi la tua frase...")
        self.turn_entry.config(fg='grey')

    def disable_turn_entry(self):
        self.turn_entry.delete(0, tk.END)
        self.turn_entry.insert(0, "Scrivi la tua frase...")
        self.turn_entry.config(state='disabled', fg='grey')
        self.turn_send_btn.config(state='disabled')

    def send_turn_phrase(self):
        phrase = self.turn_entry.get().strip()
        if not phrase or phrase == "Scrivi la tua frase...":
            return
        if len(phrase) > 30:
            messagebox.showerror("Errore", "La frase può essere lunga massimo 30 caratteri.")
            return
        # Comando: 111 <len> <frase>
        self.sock.sendall(f"111 {len(phrase):02d} {phrase}".encode())
        self.append_text(f"[Frase inviata]: {phrase}")
        # RIMOSSO: self.disable_turn_entry()
        # Ora la disabilitazione viene gestita solo da handle_feedback

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

    def update_start_btn_state(self):
        if self.is_host and self.in_lobby:
            self.start_btn.config(state='normal')
        else:
            self.start_btn.config(state='disabled')

if __name__ == "__main__":
    root = tk.Tk()
    gui = ClientGUI(root)
    root.mainloop()
