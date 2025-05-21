let context = canvas.getContext("2d");
let canvasWidth = canvas.width;
let canvasHeight = canvas.height;

const MAX_AI_SPEED = 2;
const BALL_SIZE = 5;

let ballPosition;
let ballVelocityX;
let ballVelocityY;

function startBall() {
  ballPosition = { x: 20, y: 30 };
  ballVelocityX = 4;
  ballVelocityY = 2;
}

const BAR_THICKNESS = 5;
const BAR_HEIGHT = 20;
const BAR_MARGIN = 10;

let leftBarY = 10;
let rightBarY = 30;

let leftPlayerScore = 0;
let rightPlayerScore = 0;
let isGameOver = false;

document.addEventListener("mousemove", e => {
  rightBarY = e.y - canvas.offsetTop;
});

function render() {
  context.fillStyle = "black";
  context.fillRect(0, 0, canvasWidth, canvasHeight);

  context.fillStyle = "white";
  context.fillRect(ballPosition.x, ballPosition.y, BALL_SIZE, BALL_SIZE);

  context.fillRect(BAR_MARGIN, leftBarY, BAR_THICKNESS, BAR_HEIGHT);
  context.fillRect(canvasWidth - BAR_THICKNESS - BAR_MARGIN, rightBarY, BAR_THICKNESS, BAR_HEIGHT);

  context.font = "30px monospace";
  context.textAlign = "left";
  context.fillText(leftPlayerScore.toString(), 50, 50);
  context.textAlign = "right";
  context.fillText(rightPlayerScore.toString(), canvasWidth - 50, 50);
}

function moveLeftBar() {
  let ballBounds = {
    top: ballPosition.y,
    bottom: ballPosition.y + BALL_SIZE
  };

  let barBounds = {
    top: leftBarY,
    bottom: leftBarY + BAR_HEIGHT
  };

  if (ballBounds.top < barBounds.top) {
    leftBarY -= MAX_AI_SPEED;
  } else if (ballBounds.bottom > barBounds.bottom) {
    leftBarY += MAX_AI_SPEED;
  }
}

function updateGame() {
  ballPosition.x += ballVelocityX;
  ballPosition.y += ballVelocityY;
  moveLeftBar();
}

function detectBarCollision(ball, bar) {
  return (
    ball.left < bar.right &&
    ball.right > bar.left &&
    ball.top < bar.bottom &&
    ball.bottom > bar.top
  );
}

function adjustBallAngle(distanceFromTop, distanceFromBottom) {
  if (distanceFromTop < 0) {
    ballVelocityY -= 0.5;
  } else if (distanceFromBottom < 0) {
    ballVelocityY += 0.5;
  }
}

function handleCollisions() {
  let ballBounds = {
    left: ballPosition.x,
    right: ballPosition.x + BALL_SIZE,
    top: ballPosition.y,
    bottom: ballPosition.y + BALL_SIZE
  };

  let leftBarBounds = {
    left: BAR_MARGIN,
    right: BAR_MARGIN + BAR_THICKNESS,
    top: leftBarY,
    bottom: leftBarY + BAR_HEIGHT
  };

  let rightBarBounds = {
    left: canvasWidth - BAR_THICKNESS - BAR_MARGIN,
    right: canvasWidth - BAR_MARGIN,
    top: rightBarY,
    bottom: rightBarY + BAR_HEIGHT
  };

  if (detectBarCollision(ballBounds, leftBarBounds)) {
    let distanceFromTop = ballBounds.top - leftBarBounds.top;
    let distanceFromBottom = leftBarBounds.bottom - ballBounds.bottom;
    adjustBallAngle(distanceFromTop, distanceFromBottom);
    ballVelocityX = Math.abs(ballVelocityX);
  }

  if (detectBarCollision(ballBounds, rightBarBounds)) {
    let distanceFromTop = ballBounds.top - rightBarBounds.top;
    let distanceFromBottom = rightBarBounds.bottom - ballBounds.bottom;
    adjustBallAngle(distanceFromTop, distanceFromBottom);
    ballVelocityX = -Math.abs(ballVelocityX);
  }

  if (ballBounds.left < 0) {
    rightPlayerScore++;
    startBall();
  }

  if (ballBounds.right > canvasWidth) {
    leftPlayerScore++;
    startBall();
  }

  if (leftPlayerScore > 9 || rightPlayerScore > 9) {
    isGameOver = true;
  }

  if (ballBounds.top < 0 || ballBounds.bottom > canvasHeight) {
    ballVelocityY = -ballVelocityY;
  }
}

function displayGameOver() {
  context.fillStyle = "white";
  context.font = "30px monospace";
  context.textAlign = "center";
  context.fillText("GAME OVER", canvasWidth / 2, canvasHeight / 2);
}

function gameLoop() {
  render();
  updateGame();
  handleCollisions();
  if (isGameOver) {
    render();
    displayGameOver();
  } else {
    setTimeout(gameLoop, 30);
  }
}

startBall();
gameLoop();
