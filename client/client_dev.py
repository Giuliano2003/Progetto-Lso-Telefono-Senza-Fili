import socket
import threading
import tkinter as tk
from tkinter import simpledialog, scrolledtext, messagebox, Toplevel, Listbox, END

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

        # Bottone "Crea Lobby"
        self.create_lobby_button = tk.Button(master, text="Crea Lobby", command=self.create_lobby)
        self.create_lobby_button.pack(side='left', padx=(5, 10), pady=(0, 10))

        # Bottone "Mostra Lobby"
        self.show_lobbies_button = tk.Button(master, text="Mostra Lobby", command=self.show_lobbies)
        self.show_lobbies_button.pack(side='left', padx=(5, 10), pady=(0, 10))

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

    def send_message(self):
        message = self.entry.get()
        if message:
            self.sock.sendall(message.encode())
            self.entry.delete(0, tk.END)

    # Funzione associata a "Crea Lobby"
    def create_lobby(self):
        try:
            self.sock.sendall(b"100")  # Manda "100" come bytes
        except Exception as e:
            messagebox.showerror("Send Error", str(e))

    # Funzione associata a "Mostra Lobby"
    def show_lobbies(self):
        try:
            self.sock.sendall(b"102")  # OP_GET_LOBBIES
            data = self.sock.recv(4096)
            lobbies = self.parse_lobbies(data.decode())
            self.display_lobbies_popup(lobbies)
        except Exception as e:
            messagebox.showerror("Errore", f"Errore nel recupero delle lobby: {e}")

    def parse_lobbies(self, data: str):
        # Ogni lobby è su una riga, ogni campo separato da spazio
        lobbies = []
        print(f'data: {data}')
        lines = data.strip().split('\n')
        for line in lines:
            parts = line.strip().split()
            if len(parts) >= 4:
                lobby_id = parts[0]
                host = parts[1]
                max_players = parts[2]
                player_count = parts[3]
                lobbies.append({
                    "id": lobby_id,
                    "host": host,
                    "max_players": max_players,
                    "player_count": player_count
                })
        return lobbies

    def display_lobbies_popup(self, lobbies):
        popup = Toplevel(self.master)
        popup.title("Lobby Attive")
        listbox = Listbox(popup, width=60)
        listbox.pack(padx=10, pady=10)
        print(f'lobbies: {lobbies}')
        if not lobbies:
            listbox.insert(END, "Nessuna lobby attiva.")
        else:
            for lobby in lobbies:
                # Mostra i dati secondo la nuova formattazione
                listbox.insert(
                    END,
                    f"ID: {lobby['id']} | Host: {lobby['host']} | Giocatori: {lobby['player_count']}/{lobby['max_players']}"
                )

    def receive_messages(self):
        while self.running:
            try:
                data = self.sock.recv(1024)
                if not data:
                    break
                message = data.decode()
                print(f'message: {message}')
                # Controlla se il messaggio è quello di disconnessione dell'host
                if "L'host ha abbandonato la lobby, sei stato disconesso" in message:
                    self.chat_area.configure(state='normal')
                    self.chat_area.insert(tk.END, f"{message}\n")
                    self.chat_area.configure(state='disabled')
                    self.chat_area.yview(tk.END)
                    # Mostra solo popup, NON chiudere il client
                    self.master.after(0, lambda: messagebox.showinfo("Disconnesso", message))
                else:
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
    root.protocol("WM_DELETE_WINDOW", client.on_closing)
    root.mainloop()
