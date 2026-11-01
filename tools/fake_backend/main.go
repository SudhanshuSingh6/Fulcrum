// fake_backend is a tiny HTTP server used as a backend in Fulcrum's tests.
//
//	GET  /        -> the backend's id
//	POST /echo    -> the request body, streamed back
//	GET  /health  -> 200 "ok" (never delayed or failed)
//
// --delay sleeps before each response; --error-rate makes / and /echo
// return 500 with that probability.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"math/rand/v2"
	"net"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func main() {
	port := flag.Int("port", 9001, "TCP port to listen on (127.0.0.1 and all interfaces)")
	id := flag.String("id", "backend", "id returned by GET /")
	delay := flag.Duration("delay", 0, "delay before every response on / and /echo, e.g. 500ms")
	errorRate := flag.Float64("error-rate", 0, "probability (0-1) of answering / and /echo with 500")
	flag.Parse()

	if *errorRate < 0 || *errorRate > 1 {
		log.Fatalf("--error-rate must be between 0 and 1, got %v", *errorRate)
	}

	// prelude applies --delay and --error-rate. It returns false if it already
	// answered with an error.
	prelude := func(w http.ResponseWriter) bool {
		if *delay > 0 {
			time.Sleep(*delay)
		}
		if *errorRate > 0 && rand.Float64() < *errorRate {
			http.Error(w, "injected error", http.StatusInternalServerError)
			return false
		}
		return true
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /{$}", func(w http.ResponseWriter, r *http.Request) {
		if !prelude(w) {
			return
		}
		w.Header().Set("Content-Type", "text/plain")
		io.WriteString(w, *id)
	})
	mux.HandleFunc("POST /echo", func(w http.ResponseWriter, r *http.Request) {
		if !prelude(w) {
			return
		}
		w.Header().Set("Content-Type", "application/octet-stream")
		if r.ContentLength >= 0 {
			w.Header().Set("Content-Length", fmt.Sprint(r.ContentLength))
		}
		// Stream: large uploads (M1 uses 100 MB) are never held in memory.
		// Full duplex lets us write the response while still reading the body.
		http.NewResponseController(w).EnableFullDuplex()
		if _, err := io.Copy(w, r.Body); err != nil {
			log.Printf("echo: %v", err)
		}
	})
	mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
		io.WriteString(w, "ok")
	})

	ln, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatal(err)
	}
	srv := &http.Server{Handler: mux}

	go func() {
		sig := make(chan os.Signal, 1)
		signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
		<-sig
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		srv.Shutdown(ctx)
	}()

	log.Printf("fake_backend id=%s listening on :%d", *id, *port)
	if err := srv.Serve(ln); err != nil && !errors.Is(err, http.ErrServerClosed) {
		log.Fatal(err)
	}
}
