use async_trait::async_trait;
use lettre::{AsyncSmtpTransport, AsyncTransport, Message, Tokio1Executor, message::Mailbox};

#[async_trait]
pub trait Mailer: Send + Sync {
    async fn send_link(&self, recipient: &str, subject: &str, link: &str) -> anyhow::Result<()>;
}

pub struct SmtpMailer {
    transport: AsyncSmtpTransport<Tokio1Executor>,
    from: Mailbox,
}

impl SmtpMailer {
    pub fn new(url: &str, from: &str) -> anyhow::Result<Self> {
        Ok(Self {
            transport: AsyncSmtpTransport::<Tokio1Executor>::from_url(url)?.build(),
            from: from.parse()?,
        })
    }
}

#[async_trait]
impl Mailer for SmtpMailer {
    async fn send_link(&self, recipient: &str, subject: &str, link: &str) -> anyhow::Result<()> {
        let message = Message::builder().from(self.from.clone()).to(recipient.parse()?).subject(subject).body(format!("Open this secure link to continue:\n\n{link}\n\nIf you did not request this, ignore this message."))?;
        self.transport.send(message).await?;
        Ok(())
    }
}
