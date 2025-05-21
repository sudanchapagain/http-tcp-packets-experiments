package main

import (
	"fmt"
	"log"
	"math/rand"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

const (
	port         = ":8080"
	screenWidth  = 640
	screenHeight = 480
	ballSpeed    = 4
	paddleSpeed  = 6
	timeout      = 5 * time.Minute
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

type Paddle struct {
	Y int
}

type Ball struct {
	X, Y, dxdt, dydt int
}

type Game struct {
	code     string
	player1  *websocket.Conn
	player2  *websocket.Conn
	paddle1  Paddle
	paddle2  Paddle
	ball     Ball
	mutex    sync.Mutex
	lastJoin time.Time
	started  bool
}

var sessions = make(map[string]*Game)
var sessionMutex sync.Mutex

func main() {
	http.HandleFunc("/connect", handleConnection)
	go cleanupSessions()
	log.Println("server started", port)
	log.Fatal(http.ListenAndServe(port, nil))
}

func handleConnection(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println("upgrade error:", err)
		return
	}
	defer conn.Close()

	var msg struct {
		Action string `json:"action"`
		Code   string `json:"code"`
	}

	err = conn.ReadJSON(&msg)
	if err != nil {
		log.Println("eead error:", err)
		return
	}

	if msg.Action == "join" {
		handleJoin(msg.Code, conn)
	} else if msg.Action == "create" {
		handleCreate(conn)
	}
}

func handleJoin(code string, conn *websocket.Conn) {
	sessionMutex.Lock()
	game, exists := sessions[code]
	sessionMutex.Unlock()

	if !exists || game.player2 != nil {
		conn.WriteJSON(map[string]string{"error": "Invalid code or game full"})
		return
	}

	game.mutex.Lock()
	game.player2 = conn
	game.started = true
	game.mutex.Unlock()

	log.Println("game started for:", code)
	go runGame(game)
}

func handleCreate(conn *websocket.Conn) {
	sessionMutex.Lock()
	var code string
	for {
		code = fmt.Sprintf("%04d", rand.Intn(10000))
		if _, exists := sessions[code]; !exists {
			break
		}
	}

	game := &Game{
		code:     code,
		player1:  conn,
		paddle1:  Paddle{Y: screenHeight / 2},
		paddle2:  Paddle{Y: screenHeight / 2},
		ball:     Ball{X: screenWidth / 2, Y: screenHeight / 2, dxdt: ballSpeed, dydt: ballSpeed},
		lastJoin: time.Now(),
	}

	sessions[code] = game
	sessionMutex.Unlock()

	conn.WriteJSON(map[string]string{"code": code, "message": "waiting for another player"})
}

func runGame(game *Game) {
	ticker := time.NewTicker(16 * time.Millisecond)
	defer ticker.Stop()

	for {
		<-ticker.C

		game.mutex.Lock()
		if game.player1 == nil || game.player2 == nil {
			game.mutex.Unlock()
			break
		}

		game.ball.X += game.ball.dxdt
		game.ball.Y += game.ball.dydt

		if game.ball.Y <= 0 || game.ball.Y >= screenHeight {
			game.ball.dydt = -game.ball.dydt
		}

		if game.ball.X <= 20 && game.ball.Y >= game.paddle1.Y && game.ball.Y <= game.paddle1.Y+100 {
			game.ball.dxdt = -game.ball.dxdt
		}

		if game.ball.X >= screenWidth-20 && game.ball.Y >= game.paddle2.Y && game.ball.Y <= game.paddle2.Y+100 {
			game.ball.dxdt = -game.ball.dxdt
		}

		if game.ball.X < 0 || game.ball.X > screenWidth {
			game.ball.X = screenWidth / 2
			game.ball.Y = screenHeight / 2
		}

		state := map[string]interface{}{
			"paddle1Y": game.paddle1.Y,
			"paddle2Y": game.paddle2.Y,
			"ballX":    game.ball.X,
			"ballY":    game.ball.Y,
		}

		game.player1.WriteJSON(state)
		game.player2.WriteJSON(state)
		game.mutex.Unlock()
	}
}

func cleanupSessions() {
	for {
		time.Sleep(1 * time.Minute)
		sessionMutex.Lock()
		for code, game := range sessions {
			if !game.started && time.Since(game.lastJoin) > timeout {
				delete(sessions, code)
				log.Println("Session", code, "removed due to inactivity")
			}
		}
		sessionMutex.Unlock()
	}
}
