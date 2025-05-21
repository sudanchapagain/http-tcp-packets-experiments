package main

import (
	"encoding/json"
	"fmt"
	"image/color"
	"log"
	"math"
	"net/url"
	"time"

	"github.com/gorilla/websocket"
	"github.com/hajimehoshi/bitmapfont/v3"
	"github.com/hajimehoshi/ebiten/v2"
	"github.com/hajimehoshi/ebiten/v2/text/v2"
	"github.com/hajimehoshi/ebiten/v2/vector"
)

const (
	serverAddr   = "localhost:8080"
	screenWidth  = 640
	screenHeight = 480
	paddleSpeed  = 6
	paddleWidth  = 15
	paddleHeight = 100
	ballSize     = 15
)

type Game struct {
	conn          *websocket.Conn
	playerNum     int
	paddleY       float32
	otherY        float32
	ballX         float32
	ballY         float32
	ballSpeedX    float32
	ballSpeedY    float32
	gameCode      string
	player1Score  int
	player2Score  int
	targetPaddleY float32
}

func (g *Game) Update() error {
	if ebiten.IsKeyPressed(ebiten.KeyUp) {
		g.targetPaddleY -= paddleSpeed
	}
	if ebiten.IsKeyPressed(ebiten.KeyDown) {
		g.targetPaddleY += paddleSpeed
	}

	g.paddleY += (g.targetPaddleY - g.paddleY) * 0.1

	if g.paddleY < 0 {
		g.paddleY = 0
	}
	if g.paddleY > screenHeight-paddleHeight {
		g.paddleY = screenHeight - paddleHeight
	}

	g.ballX += g.ballSpeedX
	g.ballY += g.ballSpeedY

	if g.ballY <= 0 || g.ballY+ballSize >= screenHeight {
		g.ballSpeedY = -g.ballSpeedY
	}

	if (g.ballX <= paddleWidth && g.ballY+ballSize > g.paddleY && g.ballY < g.paddleY+paddleHeight) ||
		(g.ballX+ballSize >= screenWidth-paddleWidth && g.ballY+ballSize > g.otherY && g.ballY < g.otherY+paddleHeight) {
		g.ballSpeedX = -g.ballSpeedX

		diffY := g.ballY - (g.paddleY + paddleHeight/2)
		g.ballSpeedY += diffY * 0.05
	}

	if g.ballX < 0 {
		g.player2Score++
		g.resetBall()
	} else if g.ballX+ballSize > screenWidth {
		g.player1Score++
		g.resetBall()
	}

	if g.conn != nil {
		msg := fmt.Sprintf(`{"player":%d,"y":%.2f}`, g.playerNum, g.paddleY)
		g.conn.WriteMessage(websocket.TextMessage, []byte(msg))
	}

	go func() {
		_, message, err := g.conn.ReadMessage()
		if err == nil {
			var state map[string]interface{}
			json.Unmarshal(message, &state)
			g.otherY = float32(state["paddle2Y"].(float64))
			g.ballX = float32(state["ballX"].(float64))
			g.ballY = float32(state["ballY"].(float64))
			g.ballSpeedX = float32(state["ballSpeedX"].(float64))
			g.ballSpeedY = float32(state["ballSpeedY"].(float64))
		}
	}()

	return nil
}

func (g *Game) Draw(screen *ebiten.Image) {
	screen.Fill(color.Black)

	vector.DrawFilledRect(screen, 20, g.paddleY, paddleWidth, paddleHeight, color.White, false)
	vector.DrawFilledRect(screen, screenWidth-35, g.otherY, paddleWidth, paddleHeight, color.White, false)
	vector.DrawFilledRect(screen, g.ballX, g.ballY, ballSize, ballSize, color.White, false)

	op := &text.DrawOptions{}
	op.GeoM.Translate(20, 20)
	text.Draw(screen, fmt.Sprintf("Player 1: %d", g.player1Score), text.NewGoXFace(bitmapfont.Face), op)

	op.GeoM.Translate(screenWidth-120, 40)
	text.Draw(screen, fmt.Sprintf("Player 2: %d", g.player2Score), text.NewGoXFace(bitmapfont.Face), op)
}

func (g *Game) Layout(outsideWidth, outsideHeight int) (int, int) {
	return screenWidth, screenHeight
}

func (g *Game) resetBall() {
	g.ballX = screenWidth / 2
	g.ballY = screenHeight / 2
	g.ballSpeedX = -float32(math.Abs(float64(g.ballSpeedX)))
	g.ballSpeedY = float32(math.Abs(float64(g.ballSpeedY)))
}

func main() {
	fmt.Println("enter 'create' to start a game or 'join' to enter a code:")
	var choice, code string
	fmt.Scanln(&choice)

	u := url.URL{Scheme: "ws", Host: serverAddr, Path: "/connect"}
	conn, _, err := websocket.DefaultDialer.Dial(u.String(), nil)
	if err != nil {
		log.Fatal("connection error:", err)
	}
	defer conn.Close()

	game := &Game{conn: conn, playerNum: 1}

	if choice == "create" {
		conn.WriteJSON(map[string]string{"action": "create"})
		var response map[string]string
		conn.ReadJSON(&response)
		game.gameCode = response["code"]
		fmt.Println("game created! share this code:", game.gameCode)
		fmt.Println("waiting for another player")
		time.Sleep(5 * time.Second)
	} else if choice == "join" {
		fmt.Println("game code:")
		fmt.Scanln(&code)
		conn.WriteJSON(map[string]string{"action": "join", "code": code})
		game.gameCode = code
		fmt.Println("joined game", game.gameCode)
	}

	ebiten.SetWindowSize(screenWidth, screenHeight)
	ebiten.SetWindowTitle("Pong")
	if err := ebiten.RunGame(game); err != nil {
		log.Fatal(err)
	}
}
