use authorization_api::{config::Config, state};
use std::{
    env,
    io::{Read, Write},
    net::TcpStream,
    time::Duration,
};
use tokio::net::TcpListener;
use tracing_subscriber::EnvFilter;

fn healthcheck() -> anyhow::Result<()> {
    let listen = env::var("LISTEN_ADDR").unwrap_or_else(|_| "0.0.0.0:8080".into());
    let port = listen
        .rsplit_once(':')
        .map(|(_, port)| port)
        .unwrap_or("8080");
    let mut stream = TcpStream::connect(format!("127.0.0.1:{port}"))?;
    stream.set_read_timeout(Some(Duration::from_secs(2)))?;
    stream.write_all(b"GET /health/ready HTTP/1.0\r\nHost: localhost\r\n\r\n")?;
    let mut response = [0; 64];
    let read = stream.read(&mut response)?;
    anyhow::ensure!(
        response[..read].starts_with(b"HTTP/1.0 200")
            || response[..read].starts_with(b"HTTP/1.1 200"),
        "API is not ready"
    );
    Ok(())
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    if env::args().nth(1).as_deref() == Some("--healthcheck") {
        return healthcheck();
    }
    tracing_subscriber::fmt()
        .json()
        .with_env_filter(
            EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "authorization_api=info,tower_http=info".into()),
        )
        .init();
    let config = Config::from_env()?;
    let listen = config.listen;
    let app = authorization_api::app(state(config).await?);
    let listener = TcpListener::bind(listen).await?;
    tracing::info!(%listen, "authorization API listening");
    axum::serve(listener, app.into_make_service())
        .with_graceful_shutdown(async {
            let _ = tokio::signal::ctrl_c().await;
        })
        .await?;
    Ok(())
}
