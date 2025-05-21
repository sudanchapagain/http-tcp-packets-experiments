package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"log"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

type Game struct {
	players [2]*websocket.Conn
	mutex   sync.Mutex
}

var (
	games   = make(map[string]*Game)
	gamesMu sync.Mutex
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

type Move struct {
	Piece byte
	FromX byte
	FromY byte
	ToX   byte
	ToY   byte
}

func handleConnection(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println("websocket upgrade failed:", err)
		return
	}
	defer conn.Close()

	_, gameID, err := conn.ReadMessage()
	if err != nil {
		log.Println("failed to read game id:", err)
		return
	}
	gameKey := string(gameID)

	gamesMu.Lock()
	game, exists := games[gameKey]
	if !exists {
		game = &Game{}
		games[gameKey] = game
	}
	gamesMu.Unlock()

	game.mutex.Lock()
	if game.players[0] == nil {
		game.players[0] = conn
	} else if game.players[1] == nil {
		game.players[1] = conn
	} else {
		game.mutex.Unlock()
		log.Println("game is full")
		return
	}
	game.mutex.Unlock()

	defer cleanupGame(gameKey, conn)

	for {
		_, moveData, err := conn.ReadMessage()
		if err != nil {
			log.Println("read error:", err)
			return
		}

		game.mutex.Lock()
		opponent := getOpponent(game, conn)
		game.mutex.Unlock()

		if opponent != nil {
			opponent.WriteMessage(websocket.BinaryMessage, moveData)
		}
	}
}

func getOpponent(game *Game, conn *websocket.Conn) *websocket.Conn {
	if game.players[0] == conn {
		return game.players[1]
	}
	return game.players[0]
}

func cleanupGame(gameKey string, conn *websocket.Conn) {
	gamesMu.Lock()
	defer gamesMu.Unlock()

	game, exists := games[gameKey]
	if !exists {
		return
	}

	game.mutex.Lock()
	if game.players[0] == conn {
		game.players[0] = nil
	} else if game.players[1] == conn {
		game.players[1] = nil
	}

	if game.players[0] == nil && game.players[1] == nil {
		delete(games, gameKey)
	}
	game.mutex.Unlock()
}

func encodeMove(m Move) []byte {
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, m)
	return buf.Bytes()
}

func decodeMove(data []byte) Move {
	var m Move
	binary.Read(bytes.NewReader(data), binary.LittleEndian, &m)
	return m
}

func main() {
	http.HandleFunc("/ws", handleConnection)
	server := &http.Server{
		Addr:         ":8080",
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 5 * time.Second,
	}

	fmt.Println("on 8080")
	log.Fatal(server.ListenAndServe())
}
